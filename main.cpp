#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>

// ==========================================
// 1. 纯 C++ 超微型本地 AI 意图识别引擎
// ==========================================
class MicroAI {
private:
    std::unordered_map<std::string, std::unordered_map<std::string, double>> token_probs;
    std::unordered_map<std::string, double> intent_priors;

public:
    void InitModel() {
        // 先验概率：移动、打招呼、挑衅/攻击
        intent_priors["Move"] = 0.4;
        intent_priors["Greet"] = 0.3;
        intent_priors["Attack"] = 0.3;

        // 特征词条件概率矩阵（模拟本地训练好的轻量权重）
        token_probs["Move"]["k"] = 0.4; token_probs["Move"]["j"] = 0.4;
        token_probs["Move"]["h"] = 0.4; token_probs["Move"]["l"] = 0.4;
        token_probs["Move"]["走"] = 0.3; token_probs["Move"]["移动"] = 0.3;

        token_probs["Greet"]["hello"] = 0.8; token_probs["Greet"]["嗨"] = 0.7;
        token_probs["Greet"]["有人吗"] = 0.6;

        token_probs["Attack"]["拔剑"] = 0.8; token_probs["Attack"]["决斗"] = 0.8;
        token_probs["Attack"]["垃圾"] = 0.7; token_probs["Attack"]["看招"] = 0.7;
    }

    // 极其轻量级的极速分词（按单字拆分，规避第三方复杂分词库，体积压缩到极致）
    std::vector<std::string> SuperLightTokenize(const std::string& input) {
        std::vector<std::string> tokens;
        // 支持中文字符（UTF-8中文字符通常占3字节）
        for (size_t i = 0; i < input.length(); ) {
            int len = 1;
            if ((input[i] & 0x80) == 0) len = 1;
            else if ((input[i] & 0xE0) == 0xC0) len = 2;
            else if ((input[i] & 0xF0) == 0xE0) len = 3;
            else if ((input[i] & 0xF8) == 0xF0) len = 4;
            
            tokens.push_back(input.substr(i, len));
            i += len;
        }
        return tokens;
    }

    // 本地快速贝叶斯推理
    std::string Predict(const std::string& input) {
        std::vector<std::string> words = SuperLightTokenize(input);
        std::string best_intent = "Unknown";
        double max_score = -INFINITY;

        for (auto const& [intent, prior] : intent_priors) {
            double score = std::log(prior);
            for (const auto& word : words) {
                if (token_probs[intent].count(word)) {
                    score += std::log(token_probs[intent][word]);
                } else {
                    score += std::log(0.01); // 未登录词平滑处理
                }
            }
            if (score > max_score) {
                max_score = score;
                best_intent = intent;
            }
        }
        return best_intent;
    }
};

// ==========================================
// 2. 中国象棋棋盘控制与游戏主循环
// ==========================================
struct Position { int x; int y; }; // 象棋棋盘：X为0-8(共9列)，Y为0-9(共10行)

class ChessGame {
private:
    Position player;
    Position npc;
    int npc_anger; // NPC 情绪变量：愤怒值
    MicroAI ai_brain;

    void DrawBoard() {
        std::cout << "\n====== 中国象棋微型 AI 棋盘 (9x10) ======\n";
        for (int y = 0; y < 10; ++y) {
            for (int x = 0; x < 9; ++x) {
                if (x == player.x && y == player.y) std::cout << "帅"; // 玩家
                else if (x == npc.x && y == npc.y) std::cout << "将"; // NPC
                else if (y == 4 && x == 0) std::cout << "楚"; // 汉界标识简化
                else if (y == 4 && x == 3) std::cout << "河"; // 汉界标识简化
                else if (y == 4 && x == 5) std::cout << "汉"; // 汉界标识简化
                else if (y == 4 && x == 8) std::cout << "界"; // 汉界标识简化
                else if (y == 4) std::cout << "工"; // 楚河
                else std::cout << "十";
            }
            std::cout << "\n";
        }
        std::cout << "=========================================\n";
        std::cout << "NPC 当前愤怒值: " << npc_anger << "\n";
        std::cout << "💡 提示: 输入包含'k/j/h/l'来移动，或跟NPC聊天(如:你好、垃圾)。输入'q'退出。\n";
        std::cout << "请输入您的指令: ";
    }

public:
    ChessGame() {
        std::srand(std::time(0));
        player = {4, 9}; // 玩家初始在九宫底端中央
        npc = {std::rand() % 9, std::rand() % 5}; // NPC 随机放置在敌方（k半段棋盘）
        npc_anger = 0;
        ai_brain.InitModel();
    }

    void Run() {
        std::string input;
        while (true) {
            DrawBoard();
            if (!std::getline(std::cin, input) || input == "q") {
                std::cout << "游戏退出，再见！\n";
                break;
            }
            if (input.empty()) continue;

            // 穿透到本地微型 AI 引擎进行意图识别
            std::string intent = ai_brain.Predict(input);
            std::cout << "\n[AI 引擎分析] 识别到玩家意图: " << intent << "\n";

            // 基于意图驱动本地状态机与棋盘逻辑响应
            if (intent == "Move") {
                if (input.find("k") != std::string::npos && player.y > 0) player.y--;
                else if (input.find("j") != std::string::npos && player.y < 9) player.y++;
                else if (input.find("h") != std::string::npos && player.x > 0) player.x--;
                else if (input.find("l") != std::string::npos && player.x < 8) player.x++;
                std::cout << "【系统】你迈出了沉稳的步伐，调整了棋盘站位。\n";
            } 
            else if (intent == "Greet") {
                if (npc_anger >= 50) {
                    std::cout << "【NPC 敌将】哼！现在套近乎太晚了，亮家伙吧！\n";
                } else {
                    std::cout << "【NPC 敌将】来者何人？竟敢擅闯两军阵前！\n";
                    npc_anger = std::max(0, npc_anger - 10); // 打招呼能稍微降低愤怒值
                }
            } 
            else if (intent == "Attack") {
                npc_anger += 30; // 情绪矩阵改变
                if (npc_anger >= 60) {
                    std::cout << "【NPC 敌将】大怒：“找死！看我全军突击，取你首级！”\n";
                    // 触发本地行为树：NPC 愤怒后开始主动向玩家方向位移
                    if (npc.x < player.x) npc.x++;
                    else if (npc.x > player.x) npc.x--;
                    if (npc.y < player.y) npc.y++;
                } else {
                    std::cout << "【NPC 敌将】按兵不动，冷笑道：“无知小儿，口出狂言！”\n";
                }
            } 
            else {
                std::cout << "【NPC 敌将】一脸困惑地看着你，不知道你在嘟囔什么。\n";
            }

            // 胜负判定：两军相遇
            if (player.x == npc.x && player.y == npc.y) {
                std::cout << "\n⚔️ ⚔️ ⚔️ 帅将相见！两军展开激战…… ⚔️ ⚔️ ⚔️\n";
                if (npc_anger >= 60) std::cout << "【结局】NPC处于暴走状态，战斗力翻倍，你被敌将击败了！\n";
                else std::cout << "【结局】你成功奇袭了心浮气躁的敌将，获得了胜利！\n";
                break;
            }
        }
    }
};

int main() {
    ChessGame game;
    game.Run();
    return 0;
}
