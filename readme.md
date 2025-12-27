# Tetrois

A compact, terminal-based Tetris clone written in C++ with ANSI color support and a simple highscore file.

---

## Features

- Terminal-rendered Tetris gameplay with colored blocks and a ghost piece
- Next-piece preview and a small UI panel showing score, level, lines, and highscore
- Simple scoring (standard Tetris line scores) and level progression
- Portable single-source implementation (no external libraries required)
- Game over screen and improved rendering using ncurses

## Controls

- **A**: Move left
- **D**: Move right
- **W**: Rotate
- **S**: Soft drop
- **Space**: Hard drop
- **Q**: Quit

> Note: Controls are case-sensitive and expect lowercase keys.

## Build & Run
### Prebuilt
Just go to releases and download the newest version, follow the instructions there
### Compile from scratch
1. Clone this repository

2. Compile the ncurses version (tetrois.cpp):

  ```bash
  g++ -std=c++17 tetrois.cpp -lncurses -o tetrois
  ```
  You might need to install ncurses first (Command for apt package manager)
  ```bash
  sudo apt-get install libncurses5-dev libncursesw5-dev
  ```

3. Run:

   ```bash
   ./tetrois
   ```

The code uses only the C++ standard library and POSIX terminal APIs, so any modern g++ on macOS or Linux should work.

## 💾 Highscore

The high score is stored in `highscore.txt` (a single integer). If the current score is greater than or equal to the stored highscore at game exit, `highscore.txt` will be updated.

## Troubleshooting & Tips

- Ensure your terminal supports ANSI colors and is wide enough for the UI.
- If you see rendering issues, try increasing terminal size

## License

This project is provided as-is, free and open-source. 
