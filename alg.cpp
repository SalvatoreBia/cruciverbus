#include <algorithm>
#include <array>
#include <cjson/cJSON.h>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <print>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#define CROSSWORD_FILE "crossword.txt"
#define DICTIONARY_FILE "dictionary-crossword-cleaned.csv"

extern "C" {
#include <kissat.h>
}

///////////////////////////////////////////
//                                       //
//      CROSSWORD RELATED FUNCTIONS      //
//                                       //
///////////////////////////////////////////

typedef std::vector<std::vector<char>> grid;

namespace std {
template <> struct hash<std::pair<size_t, size_t>> {
  size_t operator()(const std::pair<size_t, size_t> &p) const {
    return hash<size_t>()(p.first) ^ (hash<size_t>()(p.second) << 1);
  }
};
} // namespace std

struct DictEntry {
  std::string word;
  std::string clue;
};

grid get_input_grid(const std::string &filename) {
  grid g;
  std::ifstream file(filename);
  std::string str;
  while (std::getline(file, str)) {
    if (str.empty())
      continue;
    std::vector<char> row_vec(str.begin(), str.end());
    g.push_back(row_vec);
  }
  return g;
}

std::vector<DictEntry> get_input_dictionary(const std::string &filename) {
  std::vector<DictEntry> dict;
  std::ifstream file(filename);
  std::string str;
  bool is_header = true;

  const int MIN_CLUE_WORDS = 3;

  while (std::getline(file, str)) {
    if (str.empty())
      continue;

    if (is_header && str.find("word,clue") != std::string::npos) {
      is_header = false;
      continue;
    }

    size_t comma_pos = str.find(',');
    if (comma_pos == std::string::npos)
      continue;

    std::string word = str.substr(0, comma_pos);
    std::string clue = str.substr(comma_pos + 1);

    if (clue.size() >= 2 && clue.front() == '"' && clue.back() == '"')
      clue = clue.substr(1, clue.size() - 2);

    int word_count = 0;
    bool in_word = false;
    for (char c : clue) {
      if (c != ' ') {
        if (!in_word) {
          word_count++;
          in_word = true;
        }
      } else
        in_word = false;
    }

    if (word_count < MIN_CLUE_WORDS)
      continue;

    dict.push_back({word, clue});
  }
  std::println(std::cout,
               "Dizionario caricato. Parole valide dopo il filtro: {}",
               dict.size());

  return dict;
}

struct Cell {
  size_t row;
  size_t col;

  std::pair<size_t, size_t> get_as_pair() const {
    return std::make_pair(row, col);
  }

  bool operator<(const Cell &other) const {
    if (row != other.row)
      return row < other.row;
    return col < other.col;
  }
};

struct Slot {
  std::vector<Cell> cells;
  bool is_horizontal;
  int id;

  Slot() : is_horizontal(false), id(-1) {}
  Slot(bool flag) : is_horizontal(flag), id(-1) {}

  int get_id() const { return this->id; }

  void add_cell(size_t i, size_t j) { this->cells.push_back(Cell{i, j}); }

  int get_cell_index(size_t r, size_t c) const {
    for (size_t i = 0; i < this->cells.size(); ++i)
      if (this->cells[i].row == r && this->cells[i].col == c)
        return static_cast<int>(i);
    return -1;
  }

  void reset_cells() {
    this->cells.clear();
    this->id = -1;
  }

  size_t get_slot_size() const { return this->cells.size(); }
};

void flush_slot(std::vector<Slot> &slots, Slot &curr_slot, int &id_counter) {
  size_t size = curr_slot.get_slot_size();
  if (size > 1) {
    curr_slot.id = id_counter++;
    slots.push_back(curr_slot);
  }
  curr_slot.reset_cells();
}

std::vector<Slot> get_slots(const grid &g) {
  if (g.empty())
    return {};

  std::vector<Slot> slots;
  size_t rows = g.size();
  size_t cols = g[0].size();
  int id_counter = 0;

  for (size_t i = 0; i < rows; ++i) {
    Slot curr_slot(true);
    for (size_t j = 0; j < cols; ++j) {
      if (g[i][j] == '#')
        flush_slot(slots, curr_slot, id_counter);
      else
        curr_slot.add_cell(i, j);
    }
    flush_slot(slots, curr_slot, id_counter);
  }

  for (size_t j = 0; j < cols; ++j) {
    Slot curr_slot(false);
    for (size_t i = 0; i < rows; ++i) {
      if (g[i][j] == '#')
        flush_slot(slots, curr_slot, id_counter);
      else
        curr_slot.add_cell(i, j);
    }
    flush_slot(slots, curr_slot, id_counter);
  }

  return slots;
}

typedef std::unordered_map<std::pair<size_t, size_t>, std::array<Slot, 2>>
    intersection_map;

std::pair<size_t, size_t> get_common_element(const std::vector<Cell> &a,
                                             const std::vector<Cell> &b) {
  std::set<Cell> elements;
  for (auto v : a)
    elements.insert(v);
  for (auto v : b) {
    if (elements.count(v) > 0)
      return v.get_as_pair();
  }
  return {std::string::npos, std::string::npos};
}

intersection_map get_intersections(const std::vector<Slot> &slots) {
  intersection_map map;

  for (size_t i = 0; i < slots.size(); ++i) {
    for (size_t j = 0; j < slots.size(); ++j) {
      if (i == j)
        continue;

      std::pair<size_t, size_t> intersection =
          get_common_element(slots[i].cells, slots[j].cells);
      if (intersection.first == std::string::npos)
        continue;
      map[intersection] = {slots[i], slots[j]};
    }
  }

  return map;
}

///////////////////////////////////////////
//                                       //
//     SAT-SOLVER RELATED FUNCTIONS      //
//                                       //
///////////////////////////////////////////

class DomainManager {
  std::unordered_map<int, std::vector<DictEntry>> slot_candidates;

public:
  void initialize(const std::vector<Slot> &slots,
                  const std::vector<DictEntry> &dict) {
    std::unordered_map<size_t, std::vector<DictEntry>> dict_by_length;
    for (const auto &entry : dict) {
      dict_by_length[entry.word.size()].push_back(entry);
    }

    for (const Slot &slot : slots) {
      size_t slot_size = slot.get_slot_size();
      if (dict_by_length.count(slot_size))
        this->slot_candidates[slot.get_id()] = dict_by_length[slot_size];
    }
  }

  const std::vector<DictEntry> &get_candidates(int slot_id) const {
    static const std::vector<DictEntry> empty;
    if (!this->slot_candidates.count(slot_id))
      return empty;
    return this->slot_candidates.at(slot_id);
  }

  bool apply_consistency_filter(const intersection_map &intersections) {
    bool changed = false;
    for (const auto &[coord, slot_pair] : intersections) {
      int id1 = slot_pair[0].id;
      int id2 = slot_pair[1].id;

      if (slot_candidates[id1].empty() || slot_candidates[id2].empty())
        continue;

      const Slot &s1 = slot_pair[0];
      const Slot &s2 = slot_pair[1];

      int idx1 = s1.get_cell_index(coord.first, coord.second);
      int idx2 = s2.get_cell_index(coord.first, coord.second);

      std::set<char> valid_chars_in_s2;
      for (const auto &w : this->slot_candidates[id2])
        valid_chars_in_s2.insert(w.word[idx2]);

      auto &words1 = slot_candidates[id1];
      size_t old_size1 = words1.size();

      std::erase_if(words1, [&](const DictEntry &w) {
        return valid_chars_in_s2.find(w.word[idx1]) == valid_chars_in_s2.end();
      });

      if (words1.size() != old_size1)
        changed = true;

      std::set<char> valid_chars_in_s1;
      for (const auto &w : slot_candidates[id1])
        valid_chars_in_s1.insert(w.word[idx1]);

      auto &words2 = slot_candidates[id2];
      size_t old_size2 = words2.size();

      std::erase_if(words2, [&](const DictEntry &w) {
        return valid_chars_in_s1.find(w.word[idx2]) == valid_chars_in_s1.end();
      });

      if (words2.size() != old_size2)
        changed = true;
    }

    return changed;
  }

  void print_stats() const {
    size_t total = 0;
    for (auto &[id, list] : slot_candidates)
      total += list.size();
    std::println(std::cout, "Totale candidati rimasti: {}", total);
  }
};

class CrosswordSolver {
  struct WordVar {
    int sat_var;
    int slot_id;
    std::string word;
    std::string clue;
  };

  std::map<std::pair<size_t, size_t>, std::map<char, int>> cell_vars;

  kissat *solver;
  std::vector<WordVar> word_vars;
  std::unordered_map<int, std::vector<int>> slot_to_word_indices;
  int var_counter;

public:
  CrosswordSolver() {
    solver = kissat_init();
    kissat_set_option(solver, "quiet", 1);
    var_counter = 1;
  }

  ~CrosswordSolver() { kissat_release(solver); }

  int get_cell_var(size_t r, size_t c, char letter) {
    if (cell_vars[{r, c}].find(letter) == cell_vars[{r, c}].end()) {
      cell_vars[{r, c}][letter] = var_counter++;
    }
    return cell_vars[{r, c}][letter];
  }

  void add_at_most_one_binary(const std::vector<int> &vars) {
    int n = vars.size();
    if (n <= 1)
      return;

    int num_bits = 0;
    while ((1 << num_bits) < n)
      num_bits++;

    std::vector<int> bit_vars;
    for (int i = 0; i < num_bits; ++i)
      bit_vars.push_back(var_counter++);

    for (int i = 0; i < n; ++i) {
      int var_idx = vars[i];
      for (int b = 0; b < num_bits; ++b) {
        bool bit_val = (i >> b) & 1;

        if (bit_val) {
          kissat_add(solver, -var_idx);
          kissat_add(solver, bit_vars[b]);
          kissat_add(solver, 0);
        } else {
          kissat_add(solver, -var_idx);
          kissat_add(solver, -bit_vars[b]);
          kissat_add(solver, 0);
        }
      }
    }
  }

  void build_model(const std::vector<Slot> &slots,
                   const DomainManager &domain_mgr) {
    std::unordered_map<std::string, std::vector<int>> word_occurrences;

    for (const auto &slot : slots) {
      const auto &candidates = domain_mgr.get_candidates(slot.id);

      if (candidates.empty()) {
        kissat_add(solver, 0);
        return;
      }

      for (const auto &entry : candidates) {
        int v = var_counter++;
        word_vars.push_back({v, slot.id, entry.word, entry.clue});
        slot_to_word_indices[slot.id].push_back(word_vars.size() - 1);

        word_occurrences[entry.word].push_back(v);
      }
    }

    for (size_t i = 0; i < word_vars.size(); ++i) {
      const auto &wv = word_vars[i];
      const Slot *current_slot = nullptr;
      for (const auto &s : slots)
        if (s.id == wv.slot_id) {
          current_slot = &s;
          break;
        }
      for (size_t k = 0; k < wv.word.size(); ++k) {
        size_t r = current_slot->cells[k].row;
        size_t c = current_slot->cells[k].col;
        char letter = wv.word[k];

        int c_var = get_cell_var(r, c, letter);

        kissat_add(solver, -wv.sat_var);
        kissat_add(solver, c_var);
        kissat_add(solver, 0);
      }
    }

    for (auto const &[coord, letter_map] : cell_vars) {
      std::vector<int> lits;
      for (auto const &[l, v] : letter_map)
        lits.push_back(v);

      for (size_t i = 0; i < lits.size(); ++i) {
        for (size_t j = i + 1; j < lits.size(); ++j) {
          kissat_add(solver, -lits[i]);
          kissat_add(solver, -lits[j]);
          kissat_add(solver, 0);
        }
      }
    }

    for (const auto &slot : slots) {
      const auto &indices = slot_to_word_indices[slot.id];
      std::vector<int> sat_vars;
      for (int idx : indices)
        sat_vars.push_back(word_vars[idx].sat_var);

      for (int v : sat_vars)
        kissat_add(solver, v);
      kissat_add(solver, 0);

      add_at_most_one_binary(sat_vars);
    }

    std::println("Aggiunta vincoli unicità parole...");
    for (const auto &[word, vars] : word_occurrences) {
      if (vars.size() > 1) {
        add_at_most_one_binary(vars);
      }
    }
  }

  bool solve_and_print(const std::vector<Slot> &slots) {
    std::println(std::cout, "Avvio Kissat...");
    int res = kissat_solve(solver);
    if (res != 10) {
      std::println(std::cout, "Nessuna soluzione trovata.");
      return false;
    }
    std::println(std::cout, "Soluzione valida trovata!");
    return true;
  }

  void fill_grid(grid &g, const std::vector<Slot> &slots) {
    for (const auto &wv : word_vars) {
      if (kissat_value(solver, wv.sat_var) > 0) {
        for (const auto &s : slots) {
          if (s.id == wv.slot_id) {
            for (size_t i = 0; i < s.cells.size(); ++i)
              g[s.cells[i].row][s.cells[i].col] = wv.word[i];
            break;
          }
        }
      }
    }
  }

  void export_json(const std::string &filename, const grid &g,
                   const std::vector<Slot> &slots) {
    cJSON *root = cJSON_CreateObject();

    size_t rows = g.size();
    size_t cols = g.empty() ? 0 : g[0].size();

    cJSON_AddNumberToObject(root, "rows", rows);
    cJSON_AddNumberToObject(root, "cols", cols);

    cJSON *grid_array = cJSON_CreateArray();
    for (size_t i = 0; i < rows; ++i) {
      std::string row_str;
      for (size_t j = 0; j < cols; ++j) {
        row_str += g[i][j];
      }
      cJSON_AddItemToArray(grid_array, cJSON_CreateString(row_str.c_str()));
    }
    cJSON_AddItemToObject(root, "grid", grid_array);

    cJSON *clues_obj = cJSON_CreateObject();

    auto write_dir = [&](bool hz, const char *key) {
      cJSON *dir_array = cJSON_CreateArray();
      for (const auto &wv : word_vars) {
        if (kissat_value(solver, wv.sat_var) <= 0)
          continue;
        const Slot *s = nullptr;
        for (const auto &sl : slots) {
          if (sl.id == wv.slot_id) {
            s = &sl;
            break;
          }
        }
        if (s && s->is_horizontal == hz) {
          cJSON *clue_obj = cJSON_CreateObject();
          cJSON_AddNumberToObject(clue_obj, "id", s->id);
          cJSON_AddNumberToObject(clue_obj, "row", s->cells[0].row);
          cJSON_AddNumberToObject(clue_obj, "col", s->cells[0].col);
          cJSON_AddStringToObject(clue_obj, "word", wv.word.c_str());
          cJSON_AddStringToObject(clue_obj, "clue", wv.clue.c_str());
          cJSON_AddItemToArray(dir_array, clue_obj);
        }
      }
      cJSON_AddItemToObject(clues_obj, key, dir_array);
    };

    write_dir(true, "across");
    write_dir(false, "down");

    cJSON_AddItemToObject(root, "clues", clues_obj);

    char *json_str = cJSON_Print(root);
    std::ofstream out(filename);
    out << json_str;
    out.close();

    cJSON_free(json_str);
    cJSON_Delete(root);
  }
};

//////////////////////////////////////////////////////

int main() {
  grid crosswords = get_input_grid(CROSSWORD_FILE);
  std::vector<DictEntry> dictionary = get_input_dictionary(DICTIONARY_FILE);
  std::vector<Slot> slots = get_slots(crosswords);
  intersection_map intersections = get_intersections(slots);

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(dictionary.begin(), dictionary.end(), g);

  DomainManager domain_mgr;
  domain_mgr.initialize(slots, dictionary);

  std::println("Candidati iniziali:");
  domain_mgr.print_stats();

  int pass = 0;
  for (;;) {
    bool changed = domain_mgr.apply_consistency_filter(intersections);
    pass++;
    if (!changed)
      break;
  }

  std::println("Iterazioni filtro: {}", pass);
  domain_mgr.print_stats();

  CrosswordSolver solver;

  std::println("Costruzione modello SAT...");
  solver.build_model(slots, domain_mgr);

  if (solver.solve_and_print(slots)) {
    solver.fill_grid(crosswords, slots);
    std::println("\nGRIGLIA COMPLETATA:");
    for (const auto &row : crosswords) {
      for (char c : row)
        std::print(" {} ", c);
      std::println("");
    }
    solver.export_json("daily.json", crosswords, slots);
  }

  return 0;
}