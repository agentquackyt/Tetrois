#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h> // for terminal sizing
#include <cerrno>

// Configuration
constexpr int GRID_ROWS = 20; // Increased to standard height
constexpr int GRID_COLS = 10;
constexpr int UI_WIDTH = 13; // Fixed width for right panel content

const std::string BLOCK = "[#]";
const std::string GHOST = " # ";
const std::string CLEAN = " . ";

// ANSI Colors
const std::string RED = "\033[1;31m";
const std::string GREEN = "\033[1;32m";
const std::string YELLOW = "\033[1;33m";
const std::string BLUE = "\033[1;34m";
const std::string MAGENTA = "\033[1;35m";
const std::string CYAN = "\033[1;36m";
const std::string WHITE = "\033[1;37m";
const std::string GRAY = "\033[1;90m";
const std::string RESET = "\033[0m";

const std::vector<std::string> colors = {YELLOW, CYAN, GREEN, RED, MAGENTA, BLUE, WHITE};

// Mini-displays for the "Next" box (using full BLOCK style for consistency)
const std::vector<std::vector<std::string>> shapeDisplays = {
    {"[#][#]","[#][#]","",""},    // O
    {"[#][#][#][#]","","",""},   // I
    {" [#][#]","[#][#] ","",""}, // S
    {"[#][#] "," [#][#]","",""}, // Z
    {" [#] ","[#][#][#]","",""}, // T
    {"[#][#][#]","[#]     ","",""}, // L (adjusted spaces for alignment)
    {"[#][#][#]","    [#] ","",""}  // J (adjusted spaces for alignment)
};

struct Position {
    int x, y;
    Position(int x, int y) : x(x), y(y) {}
    Position operator+(const Position& other) const { return Position(x + other.x, y + other.y); }
    Position operator-(const Position& other) const { return Position(x - other.x, y - other.y); }
};

const Position VEC_DOWN(0, 1);
const Position VEC_LEFT(-1, 0);
const Position VEC_RIGHT(1, 0);

struct Tetromino {
    std::vector<Position> blocks;
    std::string type;
    int shapeIdx;

    Tetromino(const std::string& type, const std::vector<Position>& blocks, int idx)
        : type(type), blocks(blocks), shapeIdx(idx) {}

    void move(const Position& direction) {
        for (auto& block : blocks) block = block + direction;
    }

    void rotate() {
        if (shapeIdx == 0) return; // O-piece doesn't rotate
        Position center = blocks[0]; // Center of rotation
        for (auto& block : blocks) {
            int rel_x = block.x - center.x;
            int rel_y = block.y - center.y;
            // Standard 90 degree rotation matrix
            block.x = center.x - rel_y;
            block.y = center.y + rel_x;
        }
    }
};

struct Block {
    std::string type;
    bool occupied;
    Block() : type(RESET), occupied(false) {}
};

class Tetris {
    int rows;
    int cols;
    std::vector<Block> grid;

public:
    Tetris(int rows, int cols) : rows(rows), cols(cols) {
        grid.resize(rows * cols);
    }

    bool isInside(const Position& p) const {
        return p.x >= 0 && p.x < cols && p.y >= 0 && p.y < rows;
    }

    bool isOccupied(const Position& p) const {
        if (!isInside(p)) return true; // Walls count as occupied
        return grid[p.y * cols + p.x].occupied;
    }

    bool checkCollision(const Tetromino& t) const {
        for (const auto& b : t.blocks) {
            if (isOccupied(b)) return true;
        }
        return false;
    }

    void lockTetromino(const Tetromino& t) {
        for (const auto& b : t.blocks) {
            if (isInside(b)) {
                Block& cell = grid[b.y * cols + b.x];
                cell.occupied = true;
                cell.type = t.type;
            }
        }
    }

    int clearLines() {
        std::vector<int> linesToClear;
        for (int y = 0; y < rows; ++y) {
            bool full = true;
            for (int x = 0; x < cols; ++x) {
                if (!grid[y * cols + x].occupied) {
                    full = false;
                    break;
                }
            }
            if (full) linesToClear.push_back(y);
        }

        if (linesToClear.empty()) return 0;

        // Rebuild grid excluding cleared lines
        std::vector<Block> newGrid(rows * cols);
        int targetY = rows - 1;
        
        for (int y = rows - 1; y >= 0; --y) {
            bool isCleared = false;
            for (int clearedY : linesToClear) {
                if (y == clearedY) { isCleared = true; break; }
            }
            
            if (!isCleared) {
                for (int x = 0; x < cols; ++x) {
                    newGrid[targetY * cols + x] = grid[y * cols + x];
                }
                targetY--;
            }
        }
        grid = newGrid;
        return linesToClear.size();
    }

    // Returns a dropped version of the tetromino (Ghost Piece)
    Tetromino getGhost(Tetromino t) {
        while (!checkCollision(t)) {
            t.move(VEC_DOWN);
        }
        // Move back up one step
        t.move(Position(0, -1));
        return t;
    }

    std::string render(const Tetromino& t, int score, int level, int highscore, int lines, const Tetromino& next) {
        std::string output;
        const int CELL_W = 3; // visible width per cell
        const int DEFAULT_PANEL_W = 18; // preferred UI panel width in visible chars

        // Determine available terminal columns and clamp the panel width so lines don't wrap
        int termCols = getTerminalColumns();
        int gridVisibleWidth = cols * CELL_W + 2; // | left and right borders
        int panelW = DEFAULT_PANEL_W;
        if (termCols > 0) {
            int avail = termCols - gridVisibleWidth - 2; // space between grid and panel
            if (avail <= 0) panelW = 0; // no room for panel
            else panelW = std::min(DEFAULT_PANEL_W, avail);
        }

        // Build a grid of visible cell strings (colors included)
        std::vector<std::vector<std::string>> display(rows, std::vector<std::string>(cols, CLEAN));
        // Placed blocks
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (grid[y * cols + x].occupied) {
                    display[y][x] = grid[y * cols + x].type + BLOCK + RESET;
                }
            }
        }

        // Ghost piece (draw only on empty cells)
        Tetromino ghost = getGhost(t);
        for (const auto& b : ghost.blocks) {
            if (isInside(b) && !grid[b.y * cols + b.x].occupied) {
                display[b.y][b.x] = GRAY + GHOST + RESET;
            }
        }

        // Current tetromino (overlays ghost/placed)
        for (const auto& b : t.blocks) {
            if (isInside(b)) {
                display[b.y][b.x] = t.type + BLOCK + RESET;
            }
        }

        // Header
        output += MAGENTA + "  T E T R O I S  " + RESET + "\n";

        // Horizontal border width based on CELL_W * cols
        std::string horiz(cols * CELL_W, '-');
        if (panelW > 0) output += "+" + horiz + "+  " + std::string(panelW, ' ') + "\n";
        else output += "+" + horiz + "+\n";

        // Prepare UI panel content per row
        std::vector<std::string> ui(rows, std::string(std::max(0, panelW), ' '));
        auto setUi = [&](int row, const std::string& s){
            if (panelW <= 0) return; // no panel to write into
            if (row < 0 || row >= rows) return;
            size_t vis = visibleLen(s);
            if (vis >= (size_t)panelW) {
                // truncate visible content (strip color codes first)
                std::string stripped = stripAnsi(s);
                ui[row] = stripped.substr(0, panelW);
            } else {
                ui[row] = s + std::string(panelW - vis, ' ');
            }
        };

        setUi(1, YELLOW + "SCORE" + RESET);
        setUi(2, GREEN + std::to_string(score) + RESET);
        setUi(4, YELLOW + "LEVEL" + RESET);
        setUi(5, CYAN + std::to_string(level) + RESET);
        setUi(7, YELLOW + "LINES" + RESET);
        setUi(8, BLUE + std::to_string(lines) + RESET);
        setUi(10, YELLOW + "HIGHSCORE" + RESET);
        setUi(11, RED + std::to_string(highscore) + RESET);
        setUi(13, YELLOW + "NEXT" + RESET);

        // Render the next piece display in the UI panel
        int nextRow = 14;
        for (int i = 0; i < 4; ++i) {
            std::string s = "";
            if (i < (int)shapeDisplays[next.shapeIdx].size()) {
                s = WHITE + shapeDisplays[next.shapeIdx][i] + RESET;
            }
            setUi(nextRow + i, s);
        }

        // Controls at bottom
        setUi(rows - 5, YELLOW + "CONTROLS" + RESET);
        setUi(rows - 4, "A/D: Move");
        setUi(rows - 3, "W: Rotate");
        setUi(rows - 2, "S: Down");
        setUi(rows - 1, "Space: Drop");

        // Draw rows (always render the full grid so terminal may scroll if small)
        for (int y = 0; y < rows; ++y) {
            std::string left = "|";
            for (int x = 0; x < cols; ++x) {
                left += display[y][x];
            }
            left += "|";

            std::string uiLine = (panelW > 0 ? ui[y] : std::string());
            if (panelW > 0) {
                size_t vis = visibleLen(uiLine);
                if (vis < (size_t)panelW) uiLine += std::string(panelW - vis, ' ');
                output += left + "  " + uiLine + "\n";
            } else {
                output += left + "\n";
            }
        }

        // Bottom border
        output += "+" + horiz + "+\n";

        // If we could not render a right-side panel (stacked mode), render a compact
        // stacked UI block below the grid so important info remains visible.
        bool stacked = (panelW == 0);
        if (stacked) {
            int innerW = cols * CELL_W; // width for content inside box
            auto fmt = [&](const std::string& s){
                std::string clean = stripAnsi(s);
                if ((int)clean.size() > innerW) clean = clean.substr(0, innerW);
                return clean + std::string(innerW - (int)clean.size(), ' ');
            };

            // top border for stacked panel
            output += "+" + std::string(innerW, '-') + "+\n";

            // Core stats
            output += "|" + fmt("SCORE: " + std::to_string(score)) + "|\n";
            output += "|" + fmt("LEVEL: " + std::to_string(level)) + "|\n";
            output += "|" + fmt("LINES: " + std::to_string(lines)) + "|\n";
            output += "|" + fmt("HIGHSCORE: " + std::to_string(highscore)) + "|\n";
            output += "|" + fmt(" ") + "|\n"; // spacer

            // NEXT box
            output += "|" + fmt("NEXT") + "|\n";
            for (int i = 0; i < 4; ++i) {
                std::string s = "";
                if (i < (int)shapeDisplays[next.shapeIdx].size()) s = shapeDisplays[next.shapeIdx][i];
                output += "|" + fmt(s) + "|\n";
            }
            output += "|" + fmt(" ") + "|\n"; // spacer

            // Controls
            output += "|" + fmt("CONTROLS") + "|\n";
            output += "|" + fmt("A/D: Move") + "|\n";
            output += "|" + fmt("W: Rotate") + "|\n";
            output += "|" + fmt("S: Down") + "|\n";
            output += "|" + fmt("Space: Drop") + "|\n";

            // bottom border
            output += "+" + std::string(innerW, '-') + "+\n";
        }

        return output;
    }
private:
    static int getTerminalColumns() {
        if (const char* env = getenv("FORCE_COLS")) {
            int val = atoi(env);
            if (val > 0) return val;
        }
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            return (int)w.ws_col;
        }
        return 80; // fallback
    }

    // Get terminal rows, with optional override via FORCE_ROWS env var for testing
    static int getTerminalRows() {
        if (const char* env = getenv("FORCE_ROWS")) {
            int val = atoi(env);
            if (val > 0) return val;
        }
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
            return (int)w.ws_row;
        }
        return 24; // fallback
    }

    static std::string stripAnsi(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\033') {
                // Skip ANSI escape sequences (e.g., \033[1;31m)
                while (i < s.size() && s[i] != 'm') ++i;
                continue;
            }
            out += s[i];
        }
        return out;
    }

    static size_t visibleLen(const std::string& s) {
        return stripAnsi(s).size();
    }
};

// The main function remains unchanged
int main() {
    Tetris game(GRID_ROWS, GRID_COLS);
    bool gameOver = false;
    
    int score = 0, level = 1, totalLines = 0, highscore = 0;
    
    std::ifstream highscoreFile("highscore.txt");
    if (highscoreFile.is_open()) { highscoreFile >> highscore; highscoreFile.close(); }
    
    int lineScores[] = {0, 40, 100, 300, 1200};
    
    // Standard Tetromino definitions
    std::vector<std::vector<Position>> shapes = {
        {Position(4,0), Position(5,0), Position(4,1), Position(5,1)}, // O
        {Position(3,0), Position(4,0), Position(5,0), Position(6,0)}, // I
        {Position(5,0), Position(6,0), Position(4,1), Position(5,1)}, // S
        {Position(4,0), Position(5,0), Position(5,1), Position(6,1)}, // Z
        {Position(4,0), Position(5,0), Position(6,0), Position(5,1)}, // T
        {Position(4,0), Position(5,0), Position(6,0), Position(4,1)}, // L
        {Position(4,0), Position(5,0), Position(6,0), Position(6,1)} // J
    };
    
    srand(time(NULL));
    
    auto getNewTetromino = [&](int& idx) {
        idx = rand() % shapes.size();
        return Tetromino(colors[idx], shapes[idx], idx);
    };
    
    int currentIdx, nextIdx;
    Tetromino tetromino = getNewTetromino(currentIdx);
    Tetromino nextT = getNewTetromino(nextIdx);
    
    // Terminal Setup
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    // Hide cursor to reduce flicker
    std::cout << "\033[?25l" << std::flush;
    // Ensure cursor is restored on normal exit
    std::atexit([](){ std::cout << "\033[?25h" << std::flush; });

    auto lastDrop = std::chrono::steady_clock::now();
    int dropIntervalMs = 800;
    // Buffer to store previous frame for diffed updates
    std::vector<std::string> prevFrameLines;

    // If the RENDER_ONCE environment variable is set, print a single frame and exit (debug helper)
    if (getenv("RENDER_ONCE")) {
        std::string frame = "\033[2J\033[H" + game.render(tetromino, score, level, highscore, totalLines, nextT);
        std::cout << frame << std::flush;
        return 0;
    }
    
    while (!gameOver) {
        // Render via line-diffing: update only changed lines to minimize flicker
        std::string frame = game.render(tetromino, score, level, highscore, totalLines, nextT);
        // Split into lines (no trailing newline pieces)
        std::vector<std::string> lines;
        {
            std::string tmp;
            for (char c : frame) {
                if (c == '\n') { lines.push_back(tmp); tmp.clear(); }
                else tmp.push_back(c);
            }
            if (!tmp.empty()) lines.push_back(tmp);
        }

        // Initial full clear on first frame
        static bool firstFrame = true;
        if (firstFrame) {
            // clear screen and position cursor at top-left
            const char* clearSeq = "\033[2J\033[H";
            write(STDOUT_FILENO, clearSeq, strlen(clearSeq));
            firstFrame = false;
        }

        // Update only lines that changed
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i >= prevFrameLines.size() || lines[i] != prevFrameLines[i]) {
                // Move cursor to line (1-based) column 1 and write line then clear to EOL
                std::string out = "\033[" + std::to_string(i + 1) + ";1H" + lines[i] + "\033[K";
                // best-effort write
                ssize_t w = write(STDOUT_FILENO, out.c_str(), out.size());
                (void)w;
            }
        }
        // If previous frame was longer, clear remaining lines
        if (prevFrameLines.size() > lines.size()) {
            for (size_t i = lines.size(); i < prevFrameLines.size(); ++i) {
                std::string out = "\033[" + std::to_string(i + 1) + ";1H" + "\033[K";
                write(STDOUT_FILENO, out.c_str(), out.size());
            }
        }
        // flush
        fsync(STDOUT_FILENO);
        prevFrameLines = std::move(lines);
        // Input Handling
        char cmd = 0;
        if (read(STDIN_FILENO, &cmd, 1) > 0) {
            // Fast flush to prevent input queue lag
            char discard;
            while (read(STDIN_FILENO, &discard, 1) > 0);
            
            Tetromino temp = tetromino;
            
            if (cmd == 'q') { gameOver = true; }
            else if (cmd == 'a') { temp.move(VEC_LEFT); if (!game.checkCollision(temp)) tetromino = temp; }
            else if (cmd == 'd') { temp.move(VEC_RIGHT); if (!game.checkCollision(temp)) tetromino = temp; }
            else if (cmd == 's') { temp.move(VEC_DOWN); if (!game.checkCollision(temp)) tetromino = temp; }
            else if (cmd == 'w') {
                temp.rotate();
                if (!game.checkCollision(temp)) {
                    tetromino = temp;
                } else {
                    // Basic Wall Kick: Try moving left or right if rotation hits wall
                    temp.move(VEC_RIGHT);
                    if (!game.checkCollision(temp)) { tetromino = temp; }
                    else {
                        temp.move(VEC_LEFT); temp.move(VEC_LEFT); // Check other side
                        if (!game.checkCollision(temp)) { tetromino = temp; }
                    }
                }
            }
            // Hard Drop
            else if (cmd == ' ') {
                while(!game.checkCollision(tetromino)) {
                    tetromino.move(VEC_DOWN);
                }
                // Back up one step as it's now inside the collision
                tetromino.move(Position(0, -1));
                // Force immediate lock in next loop
                lastDrop = std::chrono::steady_clock::now() - std::chrono::seconds(10);
            }
        }
        
        // Gravity
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDrop).count() > dropIntervalMs) {
            Tetromino temp = tetromino;
            temp.move(VEC_DOWN);
            
            if (!game.checkCollision(temp)) {
                tetromino = temp;
            } else {
                game.lockTetromino(tetromino);
                int cleared = game.clearLines();
                
                if (cleared > 0) {
                    score += lineScores[cleared] * level;
                    totalLines += cleared;
                    level = (totalLines / 10) + 1;
                    dropIntervalMs = std::max(100, 800 - (level * 50));
                    if (score > highscore) highscore = score;
                }
                
                tetromino = nextT;
                nextT = getNewTetromino(nextIdx);
                
                if (game.checkCollision(tetromino)) gameOver = true;
            }
            lastDrop = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }
    
    // Restore Terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    std::cout << "\033[?25h" << std::flush; // Show cursor
    
    if (score >= highscore) {
        std::ofstream hf("highscore.txt");
        hf << score;
    }

    // Do not clear or overwrite the screen on game over. Print the "Game Over" message
    // *below* the displayed UI so the board remains visible.
    int printedLines = (int)prevFrameLines.size();
    if (printedLines <= 0) printedLines = GRID_ROWS + 2; // fallback
    std::string msg = "Game Over! Final Score: " + std::to_string(score);
    // Position cursor to the line after the last printed frame line and write message
    std::string out = "\033[" + std::to_string(printedLines + 1) + ";1H" + msg + "\n";
    write(STDOUT_FILENO, out.c_str(), out.size());

    return 0;
}