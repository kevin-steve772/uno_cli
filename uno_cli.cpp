#include "console_ui.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <random>
#include <thread>
#include <chrono>
#include <conio.h>
#include <sstream>
#include <limits>
#include <map>
#include <cstdint>

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

using namespace std;

#ifndef BG_GRAY
#define BG_GRAY 100
#endif

enum CardColor { COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_NONE };
enum CardType { NUMBER, SKIP, REVERSE, DRAW_TWO, WILD, WILD_DRAW_FOUR };

struct Card {
    CardColor color;
    CardType type;
    int number;
    Card(CardColor c = COLOR_NONE, CardType t = NUMBER, int n = 0) : color(c), type(t), number(n) {}
    string toString() const {
        if (type == NUMBER) return to_string(number);
        else if (type == SKIP) return "→";
        else if (type == REVERSE) return "↻";
        else if (type == DRAW_TWO) return "+2";
        else if (type == WILD) return "⫶";
        else if (type == WILD_DRAW_FOUR) return "+4";
        return "?";
    }
    int getColorCode() const {
        switch (color) {
            case COLOR_RED:    return RED;
            case COLOR_GREEN:  return GREEN;
            case COLOR_BLUE:   return BLUE;
            case COLOR_YELLOW: return YELLOW;
            default:           return DEFAULT;
        }
    }
    bool operator==(const Card& other) const {
        return color == other.color && type == other.type && number == other.number;
    }
};

class Player {
public:
    string name;
    bool isAI;
    vector<Card> hand;
    Player(const string& n, bool ai) : name(n), isAI(ai) {}
    void addCard(const Card& card) { hand.push_back(card); }
    void removeCard(int index) { hand.erase(hand.begin() + index); }
    int getHandSize() const { return hand.size(); }
};

class UNOGame {
private:
    vector<Player> players;
    vector<Card> drawPile, discardPile;
    int currentPlayer, direction;
    bool gameOver;
    int winnerIndex;
    int termHeight, termWidth;
    int lastCurrentPlayer;
    vector<int> lastHandSizes;
    Card lastTopCard;
    int lastDrawPileSize;
    bool firstDraw;
    int selectedCardIndex;
    int lastSelectedIndex;
    bool waitingForHumanInput;
    
    int slashKeyCount;
    chrono::steady_clock::time_point lastSlashTime;

public:
    UNOGame();
    void run();

private:
    void setupPlayers();
    void initGame();
    void createDeck();
    void shuffleDeck();
    void dealCards();
    void setupDiscardPile();
    void reshuffleDiscard();
    Card drawCard();
    bool isLegalPlay(const Card& played, const Card& top) const;
    void applyCardEffect(const Card& card);
    int getNextPlayer() const;
    int getNextPlayerAfterSkip(int skipped);
    CardColor chooseWildColor();
    bool playTurn();
    void drawFullUI();
    void updateUI();
    void updateSelection(int oldIdx, int newIdx);
    void drawBorder();
    void drawMessage(const string& msg, int color = YELLOW);
    void clearMessageArea();
    void handleHumanTurn();
    void moveSelectionLeft();
    void moveSelectionRight();
    string centerString(const string& sym) const {
        if (sym.size() == 1) return string("  ") + sym + string("  ");
        else return string(" ") + sym + string("  ");
    }
    void drawMessageNoErase(const string& msg, int color = YELLOW);
    void showCommandBox();
    void executeCommand(const string& cmd);
};

UNOGame::UNOGame() : lastCurrentPlayer(-1), lastDrawPileSize(-1), firstDraw(true), selectedCardIndex(0), lastSelectedIndex(-1), waitingForHumanInput(false),
                     slashKeyCount(0), lastSlashTime(chrono::steady_clock::now()) {
    srand(time(nullptr));
    setupPlayers();
    initGame();
}

void UNOGame::setupPlayers() {
    mvc(termw()/2-8,termh()/2-5);
    clrtxt("#   # #   #  ###", CYAN);
    mvc(termw()/2-8,termh()/2-4);
    clrtxt("#   # ##  # #   #", CYAN);
    mvc(termw()/2-8,termh()/2-3);
    clrtxt("#   # # # # #   #", CYAN);
    mvc(termw()/2-8,termh()/2-2);
    clrtxt("#   # #  ## #   #", CYAN);
    mvc(termw()/2-8,termh()/2-1);
    clrtxt(" ###  #   #  ###", CYAN);
    mvc(1,termh()/2+3);
    clrtxt("▬▬▬▬▬▬▬▬▬▬",DEFAULT,CYAN);
    for(int i=1;i<=4;i++){
        for(int j=1;j<=termw();j++){
            mvc(j,termh()/2+3);
            clrtxt("▬",DEFAULT,CYAN);
        }
        for(int j=1;j<=termw()-10;j++){
            mvc(j,termh()/2+3);
            cout<<" ";
        }
        for(int j=termw();j>=1;j--){
            mvc(j,termh()/2+3);
            clrtxt("▬",DEFAULT,CYAN);
        }
        for(int j=termw();j>=10;j--){
            mvc(j,termh()/2+3);
            cout<<" ";
        }
    }
    mvc(1,termh()/2+3);
    cout<<"          ";
    string playerName;
    sc();
    mvc(termw()/2-8,termh()/2+3);
    clrtxt("请输入你的名字: ", CYAN);
    getline(cin, playerName);
    if (playerName.empty()) playerName = "玩家";
    players.push_back(Player(playerName, false));
    
    int aiCount;
    while (true) {
        mvc(termw()/2-12,termh()/2+3);
        clrtxt("请输入AI玩家数量 (1~3): ", CYAN);
        if (cin >> aiCount && aiCount >= 1 && aiCount <= 3) break;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        mvc(termw()/2-10,termh()/2+3);
        clrtxt("请输入1到3之间的整数。\n", RED);
        this_thread::sleep_for(chrono::milliseconds(800));
    }
    hc();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    
    for (int i = 1; i <= aiCount; ++i)
        players.push_back(Player(string("AI_") + to_string(i), true));
}

void UNOGame::initGame() {
    createDeck(); shuffleDeck(); dealCards(); setupDiscardPile();
    currentPlayer = 0; direction = 1; gameOver = false; winnerIndex = -1;
    termHeight = termh(); termWidth = termw();
    lastHandSizes.resize(players.size(), -1);
}

void UNOGame::createDeck() {
    drawPile.clear();
    CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
    for (auto color : colors) {
        drawPile.push_back(Card(color, NUMBER, 0));
        for (int num = 1; num <= 9; ++num) {
            drawPile.push_back(Card(color, NUMBER, num));
            drawPile.push_back(Card(color, NUMBER, num));
        }
        for (int i = 0; i < 2; ++i) {
            drawPile.push_back(Card(color, SKIP));
            drawPile.push_back(Card(color, REVERSE));
            drawPile.push_back(Card(color, DRAW_TWO));
        }
    }
    for (int i = 0; i < 4; ++i) {
        drawPile.push_back(Card(COLOR_NONE, WILD));
        drawPile.push_back(Card(COLOR_NONE, WILD_DRAW_FOUR));
    }
}

void UNOGame::shuffleDeck() {
    static mt19937 rng(random_device{}());
    shuffle(drawPile.begin(), drawPile.end(), rng);
}

void UNOGame::dealCards() {
    for (int i = 0; i < 7; ++i)
        for (auto& player : players) {
            if (drawPile.empty()) reshuffleDiscard();
            player.addCard(drawPile.back()); drawPile.pop_back();
        }
}

void UNOGame::setupDiscardPile() {
    do {
        if (drawPile.empty()) reshuffleDiscard();
        discardPile.push_back(drawPile.back()); drawPile.pop_back();
    } while (discardPile.back().type == WILD_DRAW_FOUR);
}

void UNOGame::reshuffleDiscard() {
    if (discardPile.size() <= 1) return;
    Card top = discardPile.back(); discardPile.pop_back();
    for (const auto& card : discardPile) drawPile.push_back(card);
    discardPile.clear(); discardPile.push_back(top);
    shuffleDeck();
}

Card UNOGame::drawCard() {
    if (drawPile.empty()) reshuffleDiscard();
    Card c = drawPile.back(); drawPile.pop_back();
    return c;
}

bool UNOGame::isLegalPlay(const Card& played, const Card& top) const {
    if (played.type == WILD || played.type == WILD_DRAW_FOUR) return true;
    if (played.color == top.color) return true;
    if (played.type == top.type && played.type != NUMBER) return true;
    if (played.type == NUMBER && top.type == NUMBER && played.number == top.number) return true;
    return false;
}

void UNOGame::applyCardEffect(const Card& card) {
    if (card.type == DRAW_TWO) {
        int next = getNextPlayer();
        for (int i = 0; i < 2; ++i) players[next].addCard(drawCard());
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == WILD_DRAW_FOUR) {
        int next = getNextPlayer();
        for (int i = 0; i < 4; ++i) players[next].addCard(drawCard());
        CardColor newColor = chooseWildColor();
        discardPile.back().color = newColor;
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == SKIP) {
        int next = getNextPlayer();
        currentPlayer = getNextPlayerAfterSkip(next);
    }
    else if (card.type == REVERSE) {
        direction *= -1;
        currentPlayer = getNextPlayer();
    }
    else {
        currentPlayer = getNextPlayer();
    }
    if (currentPlayer >= (int)players.size()) currentPlayer = 0;
    if (currentPlayer < 0) currentPlayer = players.size() - 1;
}

int UNOGame::getNextPlayer() const {
    int next = currentPlayer + direction;
    if (next < 0) next = players.size() - 1;
    if (next >= (int)players.size()) next = 0;
    return next;
}

int UNOGame::getNextPlayerAfterSkip(int skipped) {
    int next = skipped + direction;
    if (next < 0) next = players.size() - 1;
    if (next >= (int)players.size()) next = 0;
    return next;
}

CardColor UNOGame::chooseWildColor() {
    if (players[currentPlayer].isAI) {
        int count[4] = {0};
        for (const auto& card : players[currentPlayer].hand) {
            if (card.color == COLOR_RED) count[0]++;
            else if (card.color == COLOR_GREEN) count[1]++;
            else if (card.color == COLOR_BLUE) count[2]++;
            else if (card.color == COLOR_YELLOW) count[3]++;
        }
        int maxIdx = 0;
        for (int i = 1; i < 4; ++i) if (count[i] > count[maxIdx]) maxIdx = i;
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        return colors[maxIdx];
    } else {
        drawMessage(string("请选择新颜色 [1]红 [2]绿 [3]蓝 [4]黄"), YELLOW);
        int choice;
        while (true) {
            char ch = _getch();
            if (ch >= '1' && ch <= '4') { choice = ch - '1'; break; }
        }
        CardColor colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW};
        clearMessageArea();
        return colors[choice];
    }
}

bool UNOGame::playTurn() {
    Player& player = players[currentPlayer];
    Card topCard = discardPile.back();
    updateUI();
    if (player.isAI) {
        drawMessage("AI思考中...", CYAN);
        mvc(15,termh());
        clrtxt("▬▬▬▬▬▬▬▬▬▬",DEFAULT,CYAN);
        for(int i=1;i<=2;i++){
            for(int j=15;j<=termw();j++){
                mvc(j,termh());
                clrtxt("▬",DEFAULT,CYAN);
            }
            for(int j=15;j<=termw()-10;j++){
                mvc(j,termh());
                cout<<" ";
            }
            for(int j=termw();j>=15;j--){
                mvc(j,termh());
                clrtxt("▬",DEFAULT,CYAN);
            }
            for(int j=termw();j>=25;j--){
                mvc(j,termh());
                cout<<" ";
            }
        }
        for (size_t i = 0; i < player.hand.size(); ++i) {
            if (isLegalPlay(player.hand[i], topCard)) {
                Card played = player.hand[i];
                player.removeCard(i);
                discardPile.push_back(played);
                drawMessage(player.name + string(" 出了: ") + played.toString(), played.getColorCode());
                this_thread::sleep_for(chrono::milliseconds(500));
                if (player.hand.empty()) { gameOver = true; winnerIndex = currentPlayer; return true; }
                applyCardEffect(played);
                return true;
            }
        }
        drawMessage(player.name + string(" 无牌可出，摸一张牌。"), MAGENTA);
        Card newCard = drawCard();
        player.addCard(newCard);
        drawMessage(string("摸到: ") + newCard.toString(), newCard.getColorCode());
        this_thread::sleep_for(chrono::milliseconds(500));
        if (isLegalPlay(newCard, topCard)) {
            drawMessage(string("新摸的牌可以出，自动出牌！"), GREEN);
            player.removeCard(player.hand.size() - 1);
            discardPile.push_back(newCard);
            if (player.hand.empty()) { gameOver = true; winnerIndex = currentPlayer; return true; }
            applyCardEffect(newCard);
        } else currentPlayer = getNextPlayer();
        return true;
    } else {
        handleHumanTurn();
        return true;
    }
}

void UNOGame::moveSelectionLeft() {
    if (players[0].hand.empty()) return;
    int oldIdx = selectedCardIndex;
    selectedCardIndex = (selectedCardIndex - 1 + (int)players[0].hand.size()) % players[0].hand.size();
    if (oldIdx != selectedCardIndex)
        updateSelection(oldIdx, selectedCardIndex);
}

void UNOGame::moveSelectionRight() {
    if (players[0].hand.empty()) return;
    int oldIdx = selectedCardIndex;
    selectedCardIndex = (selectedCardIndex + 1) % (int)players[0].hand.size();
    if (oldIdx != selectedCardIndex)
        updateSelection(oldIdx, selectedCardIndex);
}

void UNOGame::handleHumanTurn() {
    Card topCard = discardPile.back();
    selectedCardIndex = 0;
    lastSelectedIndex = -1;
    waitingForHumanInput = true;
    updateUI();
    drawMessage(string("[←][→]选中 [Enter]出牌 [Space]摸牌"), CYAN);
    while (waitingForHumanInput) {
        int key = _getch();
        
        if (key == '/') {
            auto now = chrono::steady_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(now - lastSlashTime).count() < 500) {
                slashKeyCount++;
            } else {
                slashKeyCount = 1;
            }
            lastSlashTime = now;
            if (slashKeyCount >= 7) {
                slashKeyCount = 0;
                showCommandBox();
                updateUI();
                drawMessage(string("[←][→]选中 [Enter]出牌 [Space]摸牌"), CYAN);
            }
            continue;
        } else {
            slashKeyCount = 0;
        }
        
        if (key == 224 || key == 0) {
            key = _getch();
            if (key == 75) moveSelectionLeft();
            else if (key == 77) moveSelectionRight();
        } else if (key == 13) {
            if (isLegalPlay(players[0].hand[selectedCardIndex], topCard)) {
                Card played = players[0].hand[selectedCardIndex];
                players[0].removeCard(selectedCardIndex);
                discardPile.push_back(played);
                drawMessage(string("你出了: ") + played.toString(), played.getColorCode());
                if (players[0].hand.empty()) { gameOver = true; winnerIndex = currentPlayer; waitingForHumanInput = false; return; }
                applyCardEffect(played);
                waitingForHumanInput = false; return;
            } else {
                drawMessage(string("选中的牌不能出！请选择可出的牌。"), RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                updateUI();
                drawMessage(string("[←][→]选中 [Enter]出牌 [Space]摸牌"), CYAN);
            }
        } else if (key == 32) {
            Card newCard = drawCard();
            players[0].addCard(newCard);
            drawMessage(string("你摸到了一张: ") + newCard.toString(), newCard.getColorCode());
            this_thread::sleep_for(chrono::milliseconds(800));
            if (isLegalPlay(newCard, topCard)) {
                drawMessage(string("是否出这张牌? [Y]是 [N]否"), CYAN);
                int confirm = _getch();
                if (confirm == 'y' || confirm == 'Y') {
                    players[0].removeCard(players[0].hand.size() - 1);
                    discardPile.push_back(newCard);
                    drawMessage(string("你出了刚摸的牌！"), GREEN);
                    if (players[0].hand.empty()) { gameOver = true; winnerIndex = currentPlayer; waitingForHumanInput = false; return; }
                    applyCardEffect(newCard);
                    waitingForHumanInput = false; return;
                } else {
                    drawMessage(string("保留新牌，轮到下一家。"), DEFAULT);
                    this_thread::sleep_for(chrono::milliseconds(800));
                    currentPlayer = getNextPlayer();
                    waitingForHumanInput = false; return;
                }
            } else {
                drawMessage(string("这张牌不能出，轮到下一家。"), RED);
                this_thread::sleep_for(chrono::milliseconds(800));
                currentPlayer = getNextPlayer();
                waitingForHumanInput = false; return;
            }
        }
    }
}

void UNOGame::drawBorder() {
    mvc(1, 1);
    clrtxt("╭", CYAN);
    for (int i = 2; i < termWidth; ++i) clrtxt("─", CYAN);
    clrtxt("╮", CYAN);

    for (int r = 2; r < termHeight; ++r) {
        mvc(1, r); clrtxt("│", CYAN);
        mvc(termWidth, r); clrtxt("│", CYAN);
    }

    mvc(1, termHeight-1);
    clrtxt("╰", CYAN);
    for (int i = 2; i < termWidth; ++i) clrtxt("─", CYAN);
    clrtxt("╯", CYAN);

    mvc(termWidth / 2 - 3, 1);
    clrtxt(" UNO ", WHITE, BG_RED, TS_BOLD);
}

void UNOGame::drawFullUI() {
    termHeight = termh(); termWidth = termw();
    printf("\033[2J\033[H");
    drawBorder();
    int centerX = termWidth / 2 - 7;
    int centerY = termHeight / 2 - 2;
    int n = players.size();
    int topY = 2, bottomY = termHeight - 4, leftX = 2, rightX = termWidth - 12;
    for (int i = 0; i < n; ++i) {
        int x, y;
        if (i == 0) { y = bottomY; x = leftX; }
        else if (i == 1) { y = topY + 4; x = leftX; }
        else if (i == 2) { y = topY + 4; x = rightX; }
        else { y = topY; x = termWidth/2 - 8; }
        mvc(x, y-2);
        if (players[i].isAI) clrtxt(" [AI]", MAGENTA);
    }
    updateUI();
}

void UNOGame::updateUI() {
    ostringstream oss;
    auto out_mvc = [&](int x, int y) { oss << "\033[" << y << ";" << x << "H"; };
    auto out_clrtxt = [&](const string& text, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        if (style == TS_NONE)
            oss << "\033[" << fg << ";" << bg << "m" << text << "\033[0m";
        else
            oss << "\033[" << style << ";" << fg << ";" << bg << "m" << text << "\033[0m";
    };
    auto out_fixed = [&](const string& text, int width, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        string padded = text;
        if ((int)padded.size() < width) padded.append(width - padded.size(), ' ');
        out_clrtxt(padded, fg, bg, style);
    };
    auto draw_card_face = [&](const Card& card, int x, int y, bool selected, bool legal) {
        string sym = card.toString();
        string content = centerString(sym);
        int fg = card.getColorCode();
        int bg = legal ? BG_DEFAULT : BG_GRAY;
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt(content, fg, bg);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        if (selected) {
            out_mvc(x, y+2); out_clrtxt("─────", CYAN);
        } else {
            out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        }
    };
    auto draw_card_back = [&](int x, int y) {
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt("UNO ", DEFAULT, BG_BLUE, TS_BOLD);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
    };
    auto clear_card_area = [&](int x, int y) {
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+3); out_clrtxt("     ", DEFAULT);
    };

    int n = players.size();
    int topY = 2;
    int bottomY = termHeight - 4;
    int handStartY = bottomY - 1;
    int leftX = 2, rightX = termWidth - 12;
    Card topCard = discardPile.back();

    for (int i = 0; i < n; ++i) {
        int x, y;
        if (i == 0) { y = handStartY; x = leftX; }
        else if (i == 1) { y = topY + 4; x = leftX; }
        else if (i == 2) { y = topY + 4; x = rightX; }
        else { y = topY; x = termWidth/2 - 8; }

        string playerInfo = players[i].name + " [" + to_string(players[i].getHandSize()) + "]";
        out_mvc(x, y-2);
        out_fixed(playerInfo, 40, (i == currentPlayer) ? CYAN : DEFAULT);
        if (players[i].isAI) out_clrtxt(" [AI]", MAGENTA);

        if (i == 0) {
            int maxCards = (termWidth - leftX) / 6;
            int handSize = players[0].hand.size();
            for (int k = handSize; k < maxCards; ++k) {
                clear_card_area(leftX + k * 6, y);
            }
            for (int j = 0; j < handSize; ++j) {
                int cardX = leftX + j * 6;
                if (cardX + 5 > termWidth) break;
                bool isSelected = (j == selectedCardIndex);
                bool isLegal = isLegalPlay(players[0].hand[j], topCard);
                draw_card_face(players[0].hand[j], cardX, y, isSelected, isLegal);
            }
        } else {
            int startX = x;
            int maxDisplay = 8;
            int handSize = players[i].getHandSize();
            for (int k = handSize; k < maxDisplay; ++k) {
                clear_card_area(startX + k * 6, y);
            }
            clear_card_area(startX + maxDisplay * 6, y);
            int drawCount = min(handSize, maxDisplay);
            for (int k = 0; k < drawCount; ++k) {
                draw_card_back(startX + k * 6, y);
            }
            if (handSize > maxDisplay) {
                out_mvc(startX + maxDisplay * 6, y+1);
                out_clrtxt("...", DEFAULT);
            }
        }
    }

    if (!discardPile.empty()) {
        int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
        draw_card_face(discardPile.back(), centerX, centerY, false, false);
    }

    int centerX = termWidth / 2 - 7, centerY = termHeight / 2 - 2;
    out_mvc(centerX, centerY+5);
    if (!drawPile.empty()) draw_card_back(centerX, centerY+5);
    else out_clrtxt("(空)", DEFAULT);
    lastDrawPileSize = drawPile.size();

    int dirX = termWidth-30, dirY = termHeight/2;
    out_mvc(dirX, dirY); out_clrtxt("出牌方向: ", CYAN);
    string dirText = (direction == 1) ? "顺时针 →" : "逆时针 ←";
    out_clrtxt(dirText, YELLOW);

    firstDraw = false;
    printf("%s", oss.str().c_str());
    fflush(stdout);
    
    lastSelectedIndex = selectedCardIndex;
}

void UNOGame::updateSelection(int oldIdx, int newIdx) {
    if (oldIdx == newIdx) return;
    if (players[0].hand.empty()) return;
    
    ostringstream oss;
    auto out_mvc = [&](int x, int y) { oss << "\033[" << y << ";" << x << "H"; };
    auto out_clrtxt = [&](const string& text, int fg = DEFAULT, int bg = BG_DEFAULT, int style = TS_NONE) {
        if (style == TS_NONE)
            oss << "\033[" << fg << ";" << bg << "m" << text << "\033[0m";
        else
            oss << "\033[" << style << ";" << fg << ";" << bg << "m" << text << "\033[0m";
    };
    auto draw_card_face = [&](const Card& card, int x, int y, bool selected, bool legal) {
        string sym = card.toString();
        string content = centerString(sym);
        int fg = card.getColorCode();
        int bg = legal ? BG_DEFAULT : BG_GRAY;
        out_mvc(x, y);   out_clrtxt("     ", DEFAULT);
        out_mvc(x, y+1); out_clrtxt(content, fg, bg);
        out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        if (selected) {
            out_mvc(x, y+2); out_clrtxt("─────", CYAN);
        } else {
            out_mvc(x, y+2); out_clrtxt("     ", DEFAULT);
        }
    };
    
    int bottomY = termHeight - 4;
    int handStartY = bottomY - 1;
    int leftX = 2;
    int startX = leftX;
    int cardWidth = 6;
    int y = handStartY;
    
    Card topCard = discardPile.back();
    
    if (oldIdx >= 0 && oldIdx < (int)players[0].hand.size()) {
        int cardX = startX + oldIdx * cardWidth;
        bool isLegal = isLegalPlay(players[0].hand[oldIdx], topCard);
        draw_card_face(players[0].hand[oldIdx], cardX, y, false, isLegal);
    }
    if (newIdx >= 0 && newIdx < (int)players[0].hand.size()) {
        int cardX = startX + newIdx * cardWidth;
        bool isLegal = isLegalPlay(players[0].hand[newIdx], topCard);
        draw_card_face(players[0].hand[newIdx], cardX, y, true, isLegal);
    }
    
    printf("%s", oss.str().c_str());
    fflush(stdout);
    
    lastSelectedIndex = newIdx;
}

void UNOGame::drawMessage(const string& msg, int color) {
    int msgY = termHeight;
    mvc(2, msgY); clrtxt("                                                                                ", DEFAULT, BG_DEFAULT);
    mvc(2, msgY); clrtxt(msg.c_str(), color);
}

void UNOGame::drawMessageNoErase(const string& msg, int color) {
    int msgY = termHeight;
    mvc(2, msgY); clrtxt(msg.c_str(), color);
}

void UNOGame::clearMessageArea() { drawMessage("", DEFAULT); }

void UNOGame::showCommandBox() {
    int boxW = 50;
    int boxH = 5;
    int startX = termWidth / 2 - boxW / 2;
    int startY = termHeight / 2 - boxH / 2;
    
    mvc(startX, startY);
    clrtxt("╭", CYAN);
    for (int j = 1; j < boxW - 1; ++j) clrtxt("─", CYAN);
    clrtxt("╮", CYAN);
    
    for (int i = 1; i <= boxH - 2; ++i) {
        mvc(startX, startY + i);
        clrtxt("│", CYAN);
        for (int j = 1; j < boxW - 1; ++j) clrtxt(" ", DEFAULT);
        clrtxt("│", CYAN);
    }
    
    mvc(startX, startY + boxH - 1);
    clrtxt("╰", CYAN);
    for (int j = 1; j < boxW - 1; ++j) clrtxt("─", CYAN);
    clrtxt("╯", CYAN);
    
    mvc(startX + 2, startY);
    clrtxt("控制台 [Esc]关闭", CYAN);
    mvc(startX + 2, startY + 2);
    clrtxt("> ", CYAN);
    
    string input;
    int cursorX = startX + 4;
    int cursorY = startY + 2;
    mvc(cursorX, cursorY);
    
    auto clearBox = [&]() {
        for (int i = 0; i < boxH; ++i) {
            mvc(startX, startY + i);
            for (int j = 0; j < boxW; ++j) {
                clrtxt(" ", DEFAULT);
            }
        }
    };
    sc();
    while (true) {
        int ch = _getch();
        if (ch == 27) {
            hc();
            clearBox();
            break;
        } else if (ch == 13) {
            if (!input.empty()) {
                executeCommand(input);
            }
            clearBox();
            break;
        } else if (ch == 8 || ch == 127) {
            if (!input.empty()) {
                input.pop_back();
                mvc(cursorX + (int)input.size(), cursorY);
                clrtxt(" ", DEFAULT);
                mvc(cursorX + (int)input.size(), cursorY);
            }
        } else if (ch >= 32 && ch <= 126) {
            input.push_back(static_cast<char>(ch));
            char temp[2] = {static_cast<char>(ch), '\0'};
            mvc(cursorX + (int)input.size() - 1, cursorY);
            clrtxt(temp, WHITE);
        }
    }
    hc();
    updateUI();
}

void UNOGame::executeCommand(const string& cmd) {
    istringstream iss(cmd);
    string command;
    iss >> command;
    transform(command.begin(), command.end(), command.begin(), ::tolower);
    
    if (command == "hand") {
        for (size_t i = 0; i < players.size(); ++i) {
            string msg = players[i].name + " 的手牌: ";
            for (const auto& card : players[i].hand) {
                msg += card.toString() + " ";
            }
            drawMessage(msg, CYAN);
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else if (command == "add") {
        string colorStr, typeStr;
        iss >> colorStr >> typeStr;
        if (colorStr.empty()) {
            drawMessage("用法: add <颜色> <点数或类型>", RED);
            this_thread::sleep_for(chrono::milliseconds(1500));
            clearMessageArea();
            return;
        }
        CardColor color;
        string colLower = colorStr;
        transform(colLower.begin(), colLower.end(), colLower.begin(), ::tolower);
        if (colLower == "red") color = COLOR_RED;
        else if (colLower == "green") color = COLOR_GREEN;
        else if (colLower == "blue") color = COLOR_BLUE;
        else if (colLower == "yellow") color = COLOR_YELLOW;
        else {
            drawMessage("颜色必须是 red/green/blue/yellow", RED);
            this_thread::sleep_for(chrono::milliseconds(1500));
            clearMessageArea();
            return;
        }
        
        Card newCard;
        if (typeStr == "wild") {
            newCard = Card(COLOR_NONE, WILD);
        } else if (typeStr == "+4" || typeStr == "wild+4") {
            newCard = Card(COLOR_NONE, WILD_DRAW_FOUR);
        } else if (typeStr == "skip") {
            newCard = Card(color, SKIP);
        } else if (typeStr == "reverse") {
            newCard = Card(color, REVERSE);
        } else if (typeStr == "+2") {
            newCard = Card(color, DRAW_TWO);
        } else {
            int num = atoi(typeStr.c_str());
            if (num >= 0 && num <= 9) {
                newCard = Card(color, NUMBER, num);
            } else {
                drawMessage("无效的牌型", RED);
                this_thread::sleep_for(chrono::milliseconds(1500));
                clearMessageArea();
                return;
            }
        }
        players[currentPlayer].addCard(newCard);
        drawMessage(string("已添加: ") + newCard.toString(), GREEN);
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else if (command == "skip") {
        currentPlayer = getNextPlayer();
        drawMessage("强制跳过当前玩家", YELLOW);
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else if (command == "dir") {
        direction *= -1;
        drawMessage("出牌方向已反转", YELLOW);
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else if (command == "win") {
        players[currentPlayer].hand.clear();
        gameOver = true;
        winnerIndex = currentPlayer;
        drawMessage(string("强制胜利：") + players[currentPlayer].name, GREEN);
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
        waitingForHumanInput = false;
    }
    else if (command == "reveal") {
        for (size_t i = 1; i < players.size(); ++i) {
            if (players[i].isAI) {
                string msg = players[i].name + " 的手牌: ";
                for (const auto& card : players[i].hand) {
                    msg += card.toString() + " ";
                }
                drawMessage(msg, MAGENTA);
                this_thread::sleep_for(chrono::milliseconds(1000));
            }
        }
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else if (command == "draw") {
        int count;
        iss >> count;
        if (count <= 0 || count > 20) count = 1;
        for (int i = 0; i < count; ++i) {
            players[currentPlayer].addCard(drawCard());
        }
        drawMessage(string("已摸 ") + to_string(count) + " 张牌", CYAN);
        this_thread::sleep_for(chrono::milliseconds(1500));
        clearMessageArea();
    }
    else {
        drawMessage("未知命令。可用: hand, add, skip, dir, win, reveal, draw", RED);
        this_thread::sleep_for(chrono::milliseconds(2000));
        clearMessageArea();
    }
    
    if (!gameOver) {
        updateUI();
    }
}

void UNOGame::run() {
    drawFullUI();
    while (!gameOver) {
        playTurn();
        if (gameOver) break;
        if (players[currentPlayer].isAI) this_thread::sleep_for(chrono::milliseconds(600));
    }
    mvc(termw()/2-8, termh()/2);
    string victoryMsg = string("游戏结束！胜利者是: ") + players[winnerIndex].name;
    clrtxt(victoryMsg, GREEN);
    mvc(termw()/2-8, termh()/2+1);
    clrtxt("[Enter]×2 退出", DEFAULT);
    cin.ignore(); cin.get();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    setup(); hc(); UNOGame game; game.run(); sc();
    return 0;
}