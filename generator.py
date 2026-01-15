import json
import os
import sys
import random
import subprocess
from jinja2 import Environment, FileSystemLoader

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_DIR = os.path.join(BASE_DIR, 'templates')
DATA_FILE = os.path.join(BASE_DIR, 'daily.json')
OUTPUT_FILE = os.path.join(BASE_DIR, 'index.html')
GRID_FILE = os.path.join(BASE_DIR, 'crossword.txt')
ALG_EXE = os.path.join(BASE_DIR, 'alg')

GRID_SIZE = 10
TARGET_BLACK_COUNT = 22

def is_connected(grid, white_count):
    rows = len(grid)
    cols = len(grid[0])
    start_node = None
    
    for r in range(rows):
        for c in range(cols):
            if grid[r][c] == '.':
                start_node = (r, c)
                break
        if start_node: break
    
    if not start_node:
        return white_count == 0
    
    queue = [start_node]
    visited = set([start_node])
    count = 0
    
    while queue:
        r, c = queue.pop(0)
        count += 1
        
        for dr, dc in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            nr, nc = r + dr, c + dc
            if 0 <= nr < rows and 0 <= nc < cols:
                if grid[nr][nc] == '.' and (nr, nc) not in visited:
                    visited.add((nr, nc))
                    queue.append((nr, nc))
                    
    return count == white_count

def creates_short_words(grid, r, c):
    rows = len(grid)
    cols = len(grid[0])
    
    neighbors = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    
    for dr, dc in neighbors:
        nr, nc = r + dr, c + dc
        
        if not (0 <= nr < rows and 0 <= nc < cols) or grid[nr][nc] == '#':
            continue
            
        h_len = 0
        k = nc
        while k >= 0 and grid[nr][k] == '.': 
            if (nr, k) == (r, c): break 
            h_len += 1; k -= 1
        k = nc + 1
        while k < cols and grid[nr][k] == '.':
            if (nr, k) == (r, c): break
            h_len += 1; k += 1
            
        v_len = 0
        k = nr
        while k >= 0 and grid[k][nc] == '.':
            if (k, nc) == (r, c): break
            v_len += 1; k -= 1
        k = nr + 1
        while k < rows and grid[k][nc] == '.':
            if (k, nc) == (r, c): break
            v_len += 1; k += 1
            
        if h_len < 2 and v_len < 2:
            return True 
            
    return False

def generate_crossword_grid():
    print(f"Generazione griglia {GRID_SIZE}x{GRID_SIZE}...")
    
    grid = [['.' for _ in range(GRID_SIZE)] for _ in range(GRID_SIZE)]
    current_black = 0
    total_cells = GRID_SIZE * GRID_SIZE
    
    attempts = 0
    max_attempts = 1000 
    
    while current_black < TARGET_BLACK_COUNT and attempts < max_attempts:
        r = random.randint(0, GRID_SIZE - 1)
        c = random.randint(0, GRID_SIZE - 1)
        
        if grid[r][c] == '#':
            attempts += 1
            continue
            
        sym_r = GRID_SIZE - 1 - r
        sym_c = GRID_SIZE - 1 - c
        
        grid[r][c] = '#'
        grid[sym_r][sym_c] = '#'
        
        added_count = 1 if (r == sym_r and c == sym_c) else 2
        temp_white_count = total_cells - (current_black + added_count)
        
        valid = True
        
        if creates_short_words(grid, r, c) or creates_short_words(grid, sym_r, sym_c):
            valid = False
            
        if valid and not is_connected(grid, temp_white_count):
            valid = False
            
        if valid:
            current_black += added_count
            attempts = 0 
        else:
            grid[r][c] = '.'
            grid[sym_r][sym_c] = '.'
            attempts += 1

    grid_str = ""
    for row in grid:
        grid_str += "".join(row) + "\n"
        
    return grid_str

def save_grid_file(content):
    try:
        with open(GRID_FILE, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"Griglia salvata in: {GRID_FILE}")
    except Exception as e:
        print(f"ERRORE salvataggio griglia: {e}")
        sys.exit(1)

def run_algorithm():
    print(f"Esecuzione algoritmo C++: {ALG_EXE}")
    
    if not os.path.exists(ALG_EXE):
        print(f"ERRORE: L'eseguibile {ALG_EXE} non esiste.")
        if os.path.exists(ALG_EXE + ".exe"):
            alg_cmd = ALG_EXE + ".exe"
        else:
            sys.exit(1)
    else:
        alg_cmd = ALG_EXE

    try:
        result = subprocess.run([alg_cmd], capture_output=True, text=True, encoding='utf-8', errors='replace')
        
        if result.returncode != 0:
            print("ERRORE durante l'esecuzione di ./alg:")
            print(result.stderr)
            sys.exit(result.returncode)
        else:
            print("Algoritmo C++ completato con successo.")
            print(result.stdout) 
            
    except Exception as e:
        print(f"ERRORE critico lanciando l'eseguibile: {e}")
        sys.exit(1)

def build_site():
    print(f"Lettura dati da: {DATA_FILE}")
    
    if not os.path.exists(DATA_FILE):
        print(f"ERRORE: Il file {DATA_FILE} non esiste.")
        print("L'algoritmo C++ ha generato il file JSON?")
        sys.exit(1)

    try:
        with open(DATA_FILE, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"ERRORE: Il file JSON è corrotto o malformato.\n{e}")
        sys.exit(1)

    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR))
    try:
        template = env.get_template('index.html')
    except Exception as e:
        print(f"ERRORE: Impossibile caricare il template.\n{e}")
        sys.exit(1)

    html_output = template.render(
        data=data, 
        data_json=json.dumps(data)
    )

    try:
        with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
            f.write(html_output)
        print(f"SUCCESSO: Sito generato in {OUTPUT_FILE}")
    except Exception as e:
        print(f"ERRORE durante il salvataggio del file HTML.\n{e}")
        sys.exit(1)

if __name__ == "__main__":
    grid_content = generate_crossword_grid()
    save_grid_file(grid_content)
    run_algorithm()
    build_site()
