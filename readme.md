# Tetrois

A compact, terminal-based Tetris clone written in C++ with ANSI color support and a simple highscore file.

---

## Features

- Terminal-rendered Tetris gameplay with colored blocks and a ghost piece
- Next-piece preview and a small UI panel showing score, level, lines, and highscore
- Smooth rendering via line-diff updates to minimize flicker
- Simple scoring (standard Tetris line scores) and level progression
- Portable single-source implementation (no external libraries required)

## Controls

- **A**: Move left
- **D**: Move right
- **W**: Rotate
- **S**: Soft drop
- **Space**: Hard drop
- **Q**: Quit

> Note: Controls are case-sensitive and expect lowercase keys.

## Build & Run

1. Compile (example):

   ```bash
   g++ -std=c++17 ./tetrois.cpp -o ./tetrois
   ```

2. Run:

   ```bash
   ./tetrois
   ```

The code uses only the C++ standard library and POSIX terminal APIs, so any modern g++ on macOS or Linux should work.

## Environment / Debug Helpers

- `RENDER_ONCE` — set to print a single frame and exit (useful for screenshots or tests):

  ```bash
  RENDER_ONCE=1 ./tetrois
  ```

- `FORCE_COLS` / `FORCE_ROWS` — override terminal dimensions used for layout when testing:

  ```bash
  FORCE_COLS=100 FORCE_ROWS=30 ./tetrois
  ```

## 💾 Highscore

The high score is stored in `highscore.txt` (a single integer). If the current score is greater than or equal to the stored highscore at game exit, `highscore.txt` will be updated.

## Troubleshooting & Tips

- Ensure your terminal supports ANSI colors and is wide enough for the UI.
- If you see rendering issues, try increasing terminal size or use `FORCE_COLS` / `FORCE_ROWS` for testing.

## Contributing

Feel free to open issues or submit pull requests. Ideas for improvements:

- Improve the rendering engine

## License

This project is provided as-is, free and open-source. 