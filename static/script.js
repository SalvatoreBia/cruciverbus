document.addEventListener('DOMContentLoaded', () => {
    if (typeof puzzleData === 'undefined') return;

    const cells = document.querySelectorAll('.grid-cell.white');
    const clueListItems = document.querySelectorAll('.clue-column li');

    let currentDirection = 'across';
    let activeClueId = null;

    function checkCompletion() {
        const whiteCells = document.querySelectorAll('.grid-cell.white');

        for (const cell of whiteCells) {
            const r = parseInt(cell.dataset.r);
            const c = parseInt(cell.dataset.c);
            const userValue = cell.value.toUpperCase();

            if (!userValue) {
                return false;
            }

            let correctLetter = null;

            for (const clue of puzzleData.clues.across) {
                if (clue.row === r && c >= clue.col && c < (clue.col + clue.word.length)) {
                    const index = c - clue.col;
                    correctLetter = clue.word[index].toUpperCase();
                    break;
                }
            }

            if (!correctLetter) {
                for (const clue of puzzleData.clues.down) {
                    if (clue.col === c && r >= clue.row && r < (clue.row + clue.word.length)) {
                        const index = r - clue.row;
                        correctLetter = clue.word[index].toUpperCase();
                        break;
                    }
                }
            }

            if (userValue !== correctLetter) {
                return false;
            }
        }

        return true;
    }

    function showCompletionModal() {
        const modal = document.getElementById('completion-modal');
        if (modal) {
            modal.style.display = 'flex';
        }
    }

    function closeCompletionModal() {
        const modal = document.getElementById('completion-modal');
        if (modal) {
            modal.style.display = 'none';
        }
    }

    function showErrors() {
        const whiteCells = document.querySelectorAll('.grid-cell.white');

        for (const cell of whiteCells) {
            const r = parseInt(cell.dataset.r);
            const c = parseInt(cell.dataset.c);
            const userValue = cell.value.toUpperCase();

            if (!userValue) {
                continue;
            }

            let correctLetter = null;

            for (const clue of puzzleData.clues.across) {
                if (clue.row === r && c >= clue.col && c < (clue.col + clue.word.length)) {
                    const index = c - clue.col;
                    correctLetter = clue.word[index].toUpperCase();
                    break;
                }
            }

            if (!correctLetter) {
                for (const clue of puzzleData.clues.down) {
                    if (clue.col === c && r >= clue.row && r < (clue.row + clue.word.length)) {
                        const index = r - clue.row;
                        correctLetter = clue.word[index].toUpperCase();
                        break;
                    }
                }
            }

            if (userValue !== correctLetter) {
                cell.classList.add('error');
            }
        }
    }

    function getClueByCell(r, c, direction) {
        if (direction === 'across') {
            return puzzleData.clues.across.find(cl =>
                cl.row === r && c >= cl.col && c < (cl.col + cl.word.length)
            );
        } else {
            return puzzleData.clues.down.find(cl =>
                cl.col === c && r >= cl.row && r < (cl.row + cl.word.length)
            );
        }
    }

    function updateMobileClueBar(clue) {
        const bar = document.getElementById('mobile-clue-bar');
        const text = document.getElementById('mobile-clue-text');
        if (bar && text && clue) {
            const dir = currentDirection === 'across' ? '→' : '↓';
            text.textContent = `${clue.clueNum}${dir} ${clue.clue.replace(/\r?\n?$/, '')}`;
        }
    }

    function highlightUI(clue) {
        document.querySelectorAll('.grid-cell').forEach(c => c.classList.remove('word-active'));
        document.querySelectorAll('.clue-column li').forEach(li => li.classList.remove('active-clue'));

        if (!clue) return;

        const clueEl = document.getElementById(`clue-${currentDirection}-${clue.id}`);
        if (clueEl) {
            clueEl.classList.add('active-clue');
            const parentUl = clueEl.closest('ul');
            if (parentUl) {
                const elTop = clueEl.offsetTop - parentUl.offsetTop;
                parentUl.scrollTo({ top: elTop - 50, behavior: 'smooth' });
            }
        }

        updateMobileClueBar(clue);

        const len = clue.word.length;
        for (let i = 0; i < len; i++) {
            let r = clue.row + (currentDirection === 'down' ? i : 0);
            let c = clue.col + (currentDirection === 'across' ? i : 0);
            const cell = document.querySelector(`.grid-cell[data-r="${r}"][data-c="${c}"]`);
            if (cell) cell.classList.add('word-active');
        }
    }

    function activateCell(cell, forceFlip = false) {
        if (!cell) return;

        const r = parseInt(cell.dataset.r);
        const c = parseInt(cell.dataset.c);

        const acrossClue = getClueByCell(r, c, 'across');
        const downClue = getClueByCell(r, c, 'down');

        if (forceFlip) {
            currentDirection = (currentDirection === 'across') ? 'down' : 'across';
        } else if (currentDirection === 'across' && !acrossClue && downClue) {
            currentDirection = 'down';
        } else if (currentDirection === 'down' && !downClue && acrossClue) {
            currentDirection = 'across';
        }

        const activeClue = (currentDirection === 'across') ? acrossClue : downClue;

        if (activeClue) {
            activeClueId = activeClue.id;
            highlightUI(activeClue);
        }

        cell.focus({ preventScroll: true });
    }

    function jumpToNextWord() {
        const clueList = puzzleData.clues[currentDirection];
        let currentIndex = clueList.findIndex(cl => cl.id === activeClueId);

        let nextIndex = (currentIndex + 1) % clueList.length;
        const nextClue = clueList[nextIndex];

        const nextCell = document.querySelector(`.grid-cell[data-r="${nextClue.row}"][data-c="${nextClue.col}"]`);
        if (nextCell) {
            activateCell(nextCell, false);
        }
    }

    clueListItems.forEach(li => {
        li.addEventListener('click', () => {
            const id = parseInt(li.dataset.id);
            const dir = li.dataset.dir;

            currentDirection = dir;

            const clue = puzzleData.clues[dir].find(c => c.id === id);

            if (clue) {
                const cell = document.querySelector(`.grid-cell[data-r="${clue.row}"][data-c="${clue.col}"]`);
                activateCell(cell, false);
            }
        });
    });

    cells.forEach(cell => {
        cell.addEventListener('mousedown', (e) => {
            if (document.activeElement === cell) {
                activateCell(cell, true);
            } else {
                setTimeout(() => activateCell(cell, false), 0);
            }
        });

        cell.addEventListener('keydown', (e) => {
            const r = parseInt(cell.dataset.r);
            const c = parseInt(cell.dataset.c);

            if (e.key === ' ') { e.preventDefault(); activateCell(cell, true); return; }
            if (e.key === 'Tab') { e.preventDefault(); jumpToNextWord(); return; }

            let nextCell = null;
            if (e.key === 'ArrowRight') nextCell = document.querySelector(`[data-r="${r}"][data-c="${c + 1}"]`);
            if (e.key === 'ArrowLeft') nextCell = document.querySelector(`[data-r="${r}"][data-c="${c - 1}"]`);
            if (e.key === 'ArrowDown') nextCell = document.querySelector(`[data-r="${r + 1}"][data-c="${c}"]`);
            if (e.key === 'ArrowUp') nextCell = document.querySelector(`[data-r="${r - 1}"][data-c="${c}"]`);

            if (nextCell && (e.key.startsWith('Arrow'))) {
                e.preventDefault();
                activateCell(nextCell, false);
                return;
            }

            if (e.key === 'Backspace') {
                if (cell.value === '') {
                    let prevR = r - (currentDirection === 'down' ? 1 : 0);
                    let prevC = c - (currentDirection === 'across' ? 1 : 0);
                    let prevCell = document.querySelector(`[data-r="${prevR}"][data-c="${prevC}"]`);
                    if (prevCell && prevCell.classList.contains('word-active')) {
                        prevCell.focus();
                    }
                }
            }

            if (e.key.length === 1 && /^[a-zA-Z]$/.test(e.key) && !e.ctrlKey && !e.altKey && !e.metaKey) {
                e.preventDefault();
                cell.value = e.key.toUpperCase();

                cell.classList.remove('error');

                setTimeout(() => {
                    if (checkCompletion()) {
                        showCompletionModal();
                    }
                }, 100);

                let nextR = r + (currentDirection === 'down' ? 1 : 0);
                let nextC = c + (currentDirection === 'across' ? 1 : 0);
                let autoNext = document.querySelector(`[data-r="${nextR}"][data-c="${nextC}"]`);

                if (autoNext) {
                    activateCell(autoNext, false);
                } else {
                    jumpToNextWord();
                }
            }
        });
    });

    const firstClue = puzzleData.clues.across[0];
    if (firstClue) {
        const startCell = document.querySelector(`.grid-cell[data-r="${firstClue.row}"][data-c="${firstClue.col}"]`);
        if (startCell) {
            currentDirection = 'across';
            activateCell(startCell, false);
        }
    }

    const closeModalBtn = document.getElementById('close-modal-btn');
    if (closeModalBtn) {
        closeModalBtn.addEventListener('click', closeCompletionModal);
    }

    const modal = document.getElementById('completion-modal');
    if (modal) {
        modal.addEventListener('click', (e) => {
            if (e.target === modal) {
                closeCompletionModal();
            }
        });
    }

    const showErrorsBtn = document.getElementById('show-errors-btn');
    if (showErrorsBtn) {
        showErrorsBtn.addEventListener('click', showErrors);
    }
});
