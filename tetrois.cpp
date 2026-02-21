#ifdef _WIN32
    #include <curses.h>
#else
    #include <ncurses.h>
#endif

#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

// Configuration
constexpr int GRID_ROWS = 20;
constexpr int GRID_COLS = 10;

// Visual cell strings (3 chars wide, matching the old ANSI version)
constexpr int CELL_W = 3;
const std::string BLOCK = "[#]";
const std::string GHOST = " # ";
const std::string CLEAN = " . ";

// Mini-displays for the "Next" box (plain strings; color applied by ncurses)
const std::vector<std::vector<std::string>> shapeDisplays = {
    {"[#][#]", "[#][#]", "", ""},
    {"[#][#][#][#]", "", "", ""},
    {"   [#][#]", "[#][#] ", "", ""},
    {"[#][#]   ", "   [#][#]", "", ""},
    {"   [#]", "[#][#][#]", "", ""},
    {"[#][#][#]", "[#]", "", ""},
    {"[#][#][#]", "      [#]", "", ""},
};

// ncurses color pairs
constexpr short PAIR_TITLE = 1;
constexpr short PAIR_LABEL = 2;
constexpr short PAIR_SCORE = 3;
constexpr short PAIR_LEVEL = 4;
constexpr short PAIR_LINES = 5;
constexpr short PAIR_HIGHSCORE = 6;
constexpr short PAIR_GHOST = 7;
constexpr short PAIR_PIECE_BASE = 10; // 10..16

struct Position
{
    int x;
    int y;
    Position(int x, int y) : x(x), y(y) {}

    Position operator+(const Position &other) const { return Position(x + other.x, y + other.y); }
    Position operator-(const Position &other) const { return Position(x - other.x, y - other.y); }
};

const Position VEC_DOWN(0, 1);
const Position VEC_LEFT(-1, 0);
const Position VEC_RIGHT(1, 0);

struct Tetromino
{
    std::vector<Position> blocks;
    short colorPair;
    int shapeIdx;

    Tetromino(short colorPair, const std::vector<Position> &blocks, int idx)
        : blocks(blocks), colorPair(colorPair), shapeIdx(idx) {}

    void move(const Position &direction)
    {
        for (auto &block : blocks)
            block = block + direction;
    }

    void rotate()
    {
        if (shapeIdx == 0)
            return; // O-piece doesn't rotate
        Position center = blocks[0];
        for (auto &block : blocks)
        {
            int rel_x = block.x - center.x;
            int rel_y = block.y - center.y;
            block.x = center.x - rel_y;
            block.y = center.y + rel_x;
        }
    }
};

struct Block
{
    short colorPair;
    bool occupied;
    Block() : colorPair(0), occupied(false) {}
};

class Tetris
{
    int rows;
    int cols;
    std::vector<Block> grid;

public:
    Tetris(int rows, int cols) : rows(rows), cols(cols), grid(rows * cols) {}

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    bool isInside(const Position &p) const
    {
        return p.x >= 0 && p.x < cols && p.y >= 0 && p.y < rows;
    }

    bool isOccupied(const Position &p) const
    {
        if (!isInside(p))
            return true;
        return grid[p.y * cols + p.x].occupied;
    }

    const Block &at(const Position &p) const
    {
        return grid[p.y * cols + p.x];
    }

    bool checkCollision(const Tetromino &t) const
    {
        for (const auto &b : t.blocks)
        {
            if (isOccupied(b))
                return true;
        }
        return false;
    }

    void lockTetromino(const Tetromino &t)
    {
        for (const auto &b : t.blocks)
        {
            if (!isInside(b))
                continue;
            Block &cell = grid[b.y * cols + b.x];
            cell.occupied = true;
            cell.colorPair = t.colorPair;
        }
    }

    int clearLines()
    {
        std::vector<int> linesToClear;
        for (int y = 0; y < rows; ++y)
        {
            bool full = true;
            for (int x = 0; x < cols; ++x)
            {
                if (!grid[y * cols + x].occupied)
                {
                    full = false;
                    break;
                }
            }
            if (full)
                linesToClear.push_back(y);
        }

        if (linesToClear.empty())
            return 0;

        std::vector<Block> newGrid(rows * cols);
        int targetY = rows - 1;

        for (int y = rows - 1; y >= 0; --y)
        {
            bool cleared = false;
            for (int cy : linesToClear)
            {
                if (y == cy)
                {
                    cleared = true;
                    break;
                }
            }
            if (cleared)
                continue;

            for (int x = 0; x < cols; ++x)
            {
                newGrid[targetY * cols + x] = grid[y * cols + x];
            }
            --targetY;
        }

        grid = std::move(newGrid);
        return (int)linesToClear.size();
    }

    Tetromino getGhost(Tetromino t) const
    {
        while (!checkCollision(t))
        {
            t.move(VEC_DOWN);
        }
        t.move(Position(0, -1));
        return t;
    }
};

static void initColors()
{
    if (!has_colors())
        return;
    
    start_color();
    
    // On Windows, -1 (transparency) often fails in the standard console.
    // We will use COLOR_BLACK as a safe fallback.
    short bg = COLOR_BLACK; 

    init_pair(PAIR_TITLE, COLOR_MAGENTA, bg);
    init_pair(PAIR_LABEL, COLOR_YELLOW, bg);
    init_pair(PAIR_SCORE, COLOR_GREEN, bg);
    init_pair(PAIR_LEVEL, COLOR_CYAN, bg);
    init_pair(PAIR_LINES, COLOR_BLUE, bg);
    init_pair(PAIR_HIGHSCORE, COLOR_RED, bg);
    init_pair(PAIR_GHOST, COLOR_WHITE, bg);

    init_pair(PAIR_PIECE_BASE + 0, COLOR_YELLOW, bg);
    init_pair(PAIR_PIECE_BASE + 1, COLOR_CYAN, bg);
    init_pair(PAIR_PIECE_BASE + 2, COLOR_GREEN, bg);
    init_pair(PAIR_PIECE_BASE + 3, COLOR_RED, bg);
    init_pair(PAIR_PIECE_BASE + 4, COLOR_MAGENTA, bg);
    init_pair(PAIR_PIECE_BASE + 5, COLOR_BLUE, bg);
    init_pair(PAIR_PIECE_BASE + 6, COLOR_WHITE, bg);
}

struct CursesSession
{
    CursesSession()
    {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        curs_set(0);
        initColors();
    }

    ~CursesSession() { endwin(); }
};

static void drawText(int y, int x, short pair, const std::string &s, int attrs = 0)
{
    if (pair > 0)
        attron(COLOR_PAIR(pair));
    if (attrs != 0)
        attron(attrs);
    mvaddnstr(y, x, s.c_str(), (int)s.size());
    if (attrs != 0)
        attroff(attrs);
    if (pair > 0)
        attroff(COLOR_PAIR(pair));
}

static void drawTextW(WINDOW *w, int y, int x, short pair, const std::string &s, int attrs = 0)
{
    if (pair > 0)
        wattron(w, COLOR_PAIR(pair));
    if (attrs != 0)
        wattron(w, attrs);
    mvwaddnstr(w, y, x, s.c_str(), (int)s.size());
    if (attrs != 0)
        wattroff(w, attrs);
    if (pair > 0)
        wattroff(w, COLOR_PAIR(pair));
}

static void drawCell(int y, int x, const std::string &s, short pair, int attrs = 0)
{
    if (pair > 0)
        attron(COLOR_PAIR(pair));
    if (attrs != 0)
        attron(attrs);
    mvaddnstr(y, x, s.c_str(), CELL_W);
    if (attrs != 0)
        attroff(attrs);
    if (pair > 0)
        attroff(COLOR_PAIR(pair));
}

static void drawCellW(WINDOW *w, int y, int x, const std::string &s, short pair, int attrs = 0)
{
    if (pair > 0)
        wattron(w, COLOR_PAIR(pair));
    if (attrs != 0)
        wattron(w, attrs);
    mvwaddnstr(w, y, x, s.c_str(), CELL_W);
    if (attrs != 0)
        wattroff(w, attrs);
    if (pair > 0)
        wattroff(w, COLOR_PAIR(pair));
}

static void renderFrame(
    const Tetris &game,
    const Tetromino &current,
    const Tetromino &next,
    int score,
    int level,
    int highscore,
    int lines)
{
    int termRows = 0;
    int termCols = 0;
    getmaxyx(stdscr, termRows, termCols);

    const int rows = game.getRows();
    const int cols = game.getCols();
    const int innerW = cols * CELL_W;
    const int gridW = innerW + 2;
    const int panelGap = 2;
    const int panelW = 18;

    const int titleH = 1;
    const int gridH = rows + 2;

    // Decide whether the right-side panel fits.
    const int totalWWithPanel = gridW + panelGap + panelW;
    const bool sidePanel = (termCols >= totalWWithPanel);

    // Compute total view size (for centering).
    const int stackedInnerW = innerW;
    const int stackedW = gridW;
    const int stackedLines = 16;           // SCORE/LEVEL/LINES/HIGHSCORE + blank + NEXT + 4 lines + blank + CONTROLS + 4 lines
    const int stackedH = 2 + stackedLines; // border + content + border

    const int viewW = sidePanel ? totalWWithPanel : gridW;
    const int viewH = sidePanel ? (titleH + gridH) : (titleH + gridH + stackedH);

    const int originX = std::max(0, (termCols - viewW) / 2);
    const int originY = std::max(0, (termRows - viewH) / 2);

    // Precompute current and ghost masks for fast lookup
    std::vector<std::vector<bool>> curMask(rows, std::vector<bool>(cols, false));
    for (const auto &b : current.blocks)
    {
        if (game.isInside(b))
            curMask[b.y][b.x] = true;
    }
    Tetromino ghost = game.getGhost(current);
    std::vector<std::vector<bool>> ghostMask(rows, std::vector<bool>(cols, false));
    for (const auto &b : ghost.blocks)
    {
        if (game.isInside(b))
            ghostMask[b.y][b.x] = true;
    }

    erase();

    // Title (centered within view width)
    const std::string title = "T E T R O I S";
    const int titleX = originX + std::max(0, (viewW - (int)title.size()) / 2);
    drawText(originY + 0, titleX, PAIR_TITLE, title, A_BOLD);

    // Windows
    const int gridX = originX;
    const int gridY = originY + titleH;
    WINDOW *gridWin = derwin(stdscr, gridH, gridW, gridY, gridX);
    WINDOW *panelWin = nullptr;
    WINDOW *stackedWin = nullptr;

    if (sidePanel)
    {
        const int panelX = originX + gridW + panelGap;
        const int panelY = gridY;
        panelWin = derwin(stdscr, gridH, panelW, panelY, panelX);
    }
    else
    {
        const int stackedX = originX;
        const int stackedY = gridY + gridH;
        stackedWin = derwin(stdscr, stackedH, stackedW, stackedY, stackedX);
    }

    // Grid box
    werase(gridWin);
    box(gridWin, 0, 0);

    // Grid rows
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            Position p(x, y);
            const int cellY = 1 + y;
            const int cellX = 1 + x * CELL_W;

            std::string cellStr = CLEAN;
            short cellPair = 0;
            int cellAttr = 0;

            // Placed blocks
            if (game.at(p).occupied)
            {
                cellStr = BLOCK;
                cellPair = game.at(p).colorPair;
                cellAttr = A_BOLD;
            }

            // Ghost piece on empty cells
            if (!game.at(p).occupied && ghostMask[y][x])
            {
                cellStr = GHOST;
                cellPair = PAIR_GHOST;
                cellAttr = A_DIM;
            }

            // Current piece overlays
            if (curMask[y][x])
            {
                cellStr = BLOCK;
                cellPair = current.colorPair;
                cellAttr = A_BOLD;
            }

            drawCellW(gridWin, cellY, cellX, cellStr, cellPair, cellAttr);
        }
    }

    // Panel / stacked UI
    if (sidePanel && panelWin != nullptr)
    {
        werase(panelWin);

        // y here is the grid cell row index (0..rows-1). We'll map it to panel lines.
        for (int y = 0; y < rows; ++y)
        { 
            if (y == 1)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "SCORE", A_BOLD);
            if (y == 2)
                drawTextW(panelWin, y + 1, 0, PAIR_SCORE, std::to_string(score), A_BOLD);
            if (y == 4)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "LEVEL", A_BOLD);
            if (y == 5)
                drawTextW(panelWin, y + 1, 0, PAIR_LEVEL, std::to_string(level), A_BOLD);
            if (y == 7)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "LINES", A_BOLD);
            if (y == 8)
                drawTextW(panelWin, y + 1, 0, PAIR_LINES, std::to_string(lines), A_BOLD);
            if (y == 10)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "HIGHSCORE", A_BOLD);
            if (y == 11)
                drawTextW(panelWin, y + 1, 0, PAIR_HIGHSCORE, std::to_string(highscore), A_BOLD);
            if (y == 12)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "NEXT", A_BOLD);
            if (y >= 13 && y <= 16)
            {
                int i = y - 13;
                drawTextW(panelWin, y + 1, 0, next.colorPair, shapeDisplays[next.shapeIdx][i], A_BOLD);
            }

            if (y == rows - 5)
                drawTextW(panelWin, y + 1, 0, PAIR_LABEL, "CONTROLS", A_BOLD);
            if (y == rows - 4)
                drawTextW(panelWin, y + 1, 0, 0, "A/D: Move");
            if (y == rows - 3)
                drawTextW(panelWin, y + 1, 0, 0, "W: Rotate");
            if (y == rows - 2)
                drawTextW(panelWin, y + 1, 0, 0, "S: Down");
            if (y == rows - 1)
                drawTextW(panelWin, y + 1, 0, 0, "Space: Drop");
        }
    }

    if (!sidePanel && stackedWin != nullptr)
    {
        werase(stackedWin);
        box(stackedWin, 0, 0);

        const int contentW = stackedInnerW;
        auto line = [&](int row, const std::string &content)
        {
            std::string c = content;
            if ((int)c.size() > contentW)
                c = c.substr(0, contentW);
            if ((int)c.size() < contentW)
                c += std::string(contentW - (int)c.size(), ' ');
            mvwaddnstr(stackedWin, row, 1, c.c_str(), contentW);
        };

        int r = 1;
        {
            std::string s = "SCORE: " + std::to_string(score);
            if ((int)s.size() < contentW) s += std::string(contentW - (int)s.size(), ' ');
            drawTextW(stackedWin, r++, 1, PAIR_SCORE, s, A_BOLD);
        }
        line(r++, "LEVEL: " + std::to_string(level));
        line(r++, "LINES: " + std::to_string(lines));
        line(r++, "HIGHSCORE: " + std::to_string(highscore));
        line(r++, "NEXT");
        for (int i = 0; i < 4; ++i)
            line(r++, shapeDisplays[next.shapeIdx][i]);
        line(r++, " ");
        line(r++, "CONTROLS");
        line(r++, "A/D: Move");
        line(r++, "W: Rotate");
        line(r++, "S: Down");
        line(r++, "Space: Drop");
    }

    wnoutrefresh(gridWin);
    if (panelWin)
        wnoutrefresh(panelWin);
    if (stackedWin)
        wnoutrefresh(stackedWin);

     mvaddch(termRows - 1, termCols - 1, ' '); // move cursor out of the way   
    
    doupdate();

    if (panelWin)
        delwin(panelWin);
    if (stackedWin)
        delwin(stackedWin);
    delwin(gridWin);
}

bool gameLoop() {
Tetris game(GRID_ROWS, GRID_COLS);
    bool gameOver = false;

    int score = 0;
    int level = 1;
    int totalLines = 0;
    int highscore = 0;

    std::ifstream highscoreFile("highscore.txt");
    if (highscoreFile.is_open())
    {
        highscoreFile >> highscore;
        highscoreFile.close();
    }

    int lineScores[] = {0, 40, 100, 300, 1200};

    std::vector<std::vector<Position>> shapes = {
        {Position(4, 0), Position(5, 0), Position(4, 1), Position(5, 1)},
        {Position(3, 0), Position(4, 0), Position(5, 0), Position(6, 0)},
        {Position(5, 0), Position(6, 0), Position(4, 1), Position(5, 1)},
        {Position(4, 0), Position(5, 0), Position(5, 1), Position(6, 1)},
        {Position(4, 0), Position(5, 0), Position(6, 0), Position(5, 1)},
        {Position(4, 0), Position(5, 0), Position(6, 0), Position(4, 1)},
        {Position(4, 0), Position(5, 0), Position(6, 0), Position(6, 1)},
    };

    std::srand((unsigned)std::time(nullptr));

    auto getNewTetromino = [&](int &idx)
    {
        idx = std::rand() % (int)shapes.size();
        return Tetromino((short)(PAIR_PIECE_BASE + idx), shapes[idx], idx);
    };

    int currentIdx = 0;
    int nextIdx = 0;
    Tetromino tetromino = getNewTetromino(currentIdx);
    Tetromino nextT = getNewTetromino(nextIdx);

    int finalScore = 0;
    bool restartRequested = false;
    {
        CursesSession curses;

        auto lastDrop = std::chrono::steady_clock::now();
        int dropIntervalMs = 800;

        if (getenv("RENDER_ONCE"))
        {
            renderFrame(game, tetromino, nextT, score, level, highscore, totalLines);
            finalScore = score;
            return 0;
        }

        while (!gameOver)
        {
            renderFrame(game, tetromino, nextT, score, level, highscore, totalLines);

            int ch = getch();
            if (ch != ERR)
            {
                Tetromino temp = tetromino;

                if (ch == 'q')
                {
                    gameOver = true;
                }
                else if (ch == 'a' || ch == KEY_LEFT)
                {
                    temp.move(VEC_LEFT);
                    if (!game.checkCollision(temp))
                        tetromino = temp;
                }
                else if (ch == 'd' || ch == KEY_RIGHT)
                {
                    temp.move(VEC_RIGHT);
                    if (!game.checkCollision(temp))
                        tetromino = temp;
                }
                else if (ch == 's' || ch == KEY_DOWN)
                {
                    temp.move(VEC_DOWN);
                    if (!game.checkCollision(temp))
                        tetromino = temp;
                }
                else if (ch == 'w' || ch == KEY_UP)
                {
                    temp.rotate();
                    if (!game.checkCollision(temp))
                    {
                        tetromino = temp;
                    }
                    else
                    {
                        Tetromino kicked = temp;
                        kicked.move(VEC_RIGHT);
                        if (!game.checkCollision(kicked))
                        {
                            tetromino = kicked;
                        }
                        else
                        {
                            kicked = temp;
                            kicked.move(VEC_LEFT);
                            kicked.move(VEC_LEFT);
                            if (!game.checkCollision(kicked))
                                tetromino = kicked;
                        }
                    }
                }
                else if (ch == ' ')
                {
                    while (!game.checkCollision(tetromino))
                    {
                        tetromino.move(VEC_DOWN);
                    }
                    tetromino.move(Position(0, -1));
                    lastDrop = std::chrono::steady_clock::now() - std::chrono::seconds(10);
                }
            }

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDrop).count() > dropIntervalMs)
            {
                Tetromino temp = tetromino;
                temp.move(VEC_DOWN);

                if (!game.checkCollision(temp))
                {
                    tetromino = temp;
                }
                else
                {
                    game.lockTetromino(tetromino);
                    int cleared = game.clearLines();

                    if (cleared > 0)
                    {
                        score += lineScores[cleared] * level;
                        totalLines += cleared;
                        level = (totalLines / 10) + 1;
                        dropIntervalMs = std::max(100, 800 - (level * 50));
                        if (score > highscore)
                            highscore = score;
                    }

                    tetromino = nextT;
                    nextT = getNewTetromino(nextIdx);

                    if (game.checkCollision(tetromino))
                        gameOver = true;
                }

                lastDrop = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(90));
        }

        // Show a full-screen Game Over screen and wait for user input
        // (stay inside the curses session so it's full-screen)
        auto showGameOverScreen = [&](int finalScoreDisplay) -> bool
        {
            // ASCII art for "GAME OVER" (simple, monospaced)
            const std::vector<std::string> art = {
                " $$$$$$\\                                           $$$$$$\\                                 $$\\ ",
                "$$  __$$\\                                         $$  __$$\\                                $$ |",
                "$$ /  \\__| $$$$$$\\  $$$$$$\\$$$$\\   $$$$$$\\        $$ /  $$ |$$\\    $$\\  $$$$$$\\   $$$$$$\\  $$ |",
                "$$ |$$$$\\  \\____$$\\ $$  _$$  _$$\\ $$  __$$\\       $$ |  $$ |\\$$\\  $$  |$$  __$$\\ $$  __$$\\ $$ |",
                "$$ |\\_$$ | $$$$$$$ |$$ / $$ / $$ |$$$$$$$$ |      $$ |  $$ | \\$$\\$$  / $$$$$$$$ |$$ |  \\__|\\__|",
                "$$ |  $$ |$$  __$$ |$$ | $$ | $$ |$$   ____|      $$ |  $$ |  \\$$$  /  $$   ____|$$ |          ",
                "\\$$$$$$  |\\$$$$$$$ |$$ | $$ | $$ |\\$$$$$$$\\        $$$$$$  |   \\$  /   \\$$$$$$$\\ $$ |      $$\\ ",
                " \\______/  \\_______|\\__| \\__| \\__| \\_______|       \\______/     \\_/     \\_______|\\__|      \\__|"

            };

            int rows, cols;
            getmaxyx(stdscr, rows, cols);
            int artW = 0;
            for (auto &l : art)
                artW = std::max(artW, (int)l.size());

            int startY = std::max(0, (rows - (int)art.size() - 4) / 2);
            int startX = std::max(0, (cols - artW) / 2);

            // Clear and draw
            werase(stdscr);
            attron(A_BOLD | COLOR_PAIR(PAIR_HIGHSCORE));
            for (size_t i = 0; i < art.size(); ++i)
            {
                mvaddnstr(startY + (int)i, startX, art[i].c_str(), art[i].size());
            }
            attroff(A_BOLD | COLOR_PAIR(PAIR_HIGHSCORE));

            // Show score
            std::string scoreLine = "Final Score: " + std::to_string(finalScoreDisplay);
            drawText(startY + (int)art.size() + 1, std::max(0, (cols - (int)scoreLine.size()) / 2), PAIR_SCORE, scoreLine, A_BOLD);

            // Prompt (Space restarts)
            std::string prompt = "Press Space to restart, any other key to exit";
            drawText(startY + (int)art.size() + 3, std::max(0, (cols - (int)prompt.size()) / 2), PAIR_LABEL, prompt, A_BOLD);

            // Ignore input for the first 1 second, then accept a key and return whether it was Space
            nodelay(stdscr, TRUE);
            auto start = std::chrono::steady_clock::now();
            int pressed = ERR;
            while (true) {
                int c = getch();
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (ms < 500) {
                    // during the initial 1s, discard any keys
                    if (c != ERR) {
                        // consume and ignore
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }
                // After 1s, if a key is already waiting, consume it immediately; otherwise block until one
                if (c != ERR) { pressed = c; break; }
                nodelay(stdscr, FALSE);
                pressed = getch();
                nodelay(stdscr, TRUE);
                break;
            }

            return (pressed == ' ');
        };

        restartRequested = showGameOverScreen(score);
        finalScore = score;
    } // endwin() here

    if (finalScore >= highscore)
    {
        std::ofstream hf("highscore.txt");
        hf << finalScore;
        hf.close();
    }

    return restartRequested;
}

int main()
{   
    bool is_running{true};
    do
    {
        is_running = gameLoop();
    } while (is_running);
    return 0;
}