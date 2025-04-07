#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <vector>

#define MAX_PLAYERS 5
#define MAX_ROUNDS 3
#define SERVER_PORT 53000
#define BUFFER_SIZE 1024

int roundCnt = 1;
int familyMoney = 100, familyLove = 100, familyConnection = 100;
bool rolesLocked = false;

pthread_mutex_t mutx;
std::map<int, std::string> playerRoles;
std::map<int, int> playerScores;
std::vector<int> clientSockets;
std::map<int, int> playerSockets;
std::map<int, int> currentAnswers;
bool questionActive = false;
volatile bool gameStarted = false;

struct ClientData {
  int client;
  int playerId;
};

struct Outcome {
  int loveChange;
  int moneyChange;
  int connectChange;
  int scoreChange;
};

struct Question {
  std::string text;
  std::vector<std::string> options;
  std::vector<Outcome> outcomes;
};

std::map<std::string, std::vector<Question>> roleQuestions;

void sendMessage(int client, const std::string &msg) {
  send(client, msg.c_str(), msg.size() + 1, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

void broadcastMessage(const std::string &message) {
  pthread_mutex_lock(&mutx);
  for (int client : clientSockets) {
    sendMessage(client, message);
  }
  pthread_mutex_unlock(&mutx);
}

void broadcastGameState() {
  std::ostringstream oss;
  oss << "Раунд: " << roundCnt << " | Деньги: " << familyMoney
      << "% | Любовь: " << familyLove << "% | Связь: " << familyConnection
      << "%";
  broadcastMessage(oss.str());
}

void initQuestions() {
  Question q;
  q.text =
      "Семье нужны деньги на первичные расходы(коляска, кроватка...), каких проблем вы подкинете семье в этой ситуации?";
  q.options = {"Мужа понизят в должности на работе",
               "Ребенок разбил телефон(специально)", "Муж попал в аварию"};
  q.outcomes.clear();
  q.outcomes.push_back({0, -15, 0, 0});
  q.outcomes.push_back({0, -5, -5, 0});
  q.outcomes.push_back({0, -5, 5, 0});
  roleQuestions["1"].push_back(q);

  q.text = "Приближаются роды. Какие проблемы вы создадите?";
  q.options = {"Подозрение на проблемы у ребенка", "Агрессивные врачи",
               "Семью вызывают в школу из за плохих оценок ребенка"};
  q.outcomes.clear();
  q.outcomes.push_back({-5, -10, -10, 0});
  q.outcomes.push_back({0, 0, -15, 0});
  q.outcomes.push_back({0, -5, -5, 0});
  roleQuestions["1"].push_back(q);

  q.text = "Вот и наступили роды. Что ещё можно испортить в жизни семьи?";
  q.options = {"Ребенок и кот подрались и получили травмы",
               "Врачи будут плохо готовы к родам",
               "Муж не сможет встретить жену и сделать сюрприз после родов"};
  q.outcomes.clear();
  q.outcomes.push_back({-5, -10, -5, 0});
  q.outcomes.push_back({-5, 0, -5, 0});
  q.outcomes.push_back({-15, 0, -15, 0});
  roleQuestions["1"].push_back(q);

  q.text = "Жена говорит вам, что она беременна. Ваши действия?";
  q.options = {"Порадоватся", "Попросить в будущем сделать тест на отцовство",
               "Поругаться, ведь у вас уже есть ребенок"};
  q.outcomes.clear();
  q.outcomes.push_back({10, -5, 10, 5});
  q.outcomes.push_back({-25, -5, -20, 0});
  q.outcomes.push_back({-15, 0, -20, 10});
  roleQuestions["2"].push_back(q);

  q.text = "Вы понимаете что вам нужно больше денег чтобы накопить на первые "
           "расходы при рождении ребенка, ваши действия?";
  q.options = {"Попрошу у начальника дополнительные смены",
               "Не буду ничего делать", "Возьму денег у друга"};
  q.outcomes.clear();
  q.outcomes.push_back({-5, 5, -5, 5});
  q.outcomes.push_back({-10, -5, -15, 2});
  q.outcomes.push_back({-5, -5, 0, 10});
  roleQuestions["2"].push_back(q);

  q.text = "Жена очень волнуется и хочет, чтобы вы присутствовали на родах";
  q.options = {"Соглашусь", "Скажу, что не хочу", "Совру"};
  q.outcomes.clear();
  q.outcomes.push_back({10, 0, 10, 5});
  q.outcomes.push_back({-10, 0, -10, 2});
  q.outcomes.push_back({-15, 0, -20, 10});
  roleQuestions["2"].push_back(q);

  q.text = "Как рассказать мужу о том, что беременна";
  q.options = {"Просто показать тест", "Сделать сюрприз",
               "Разозлиться, потребовать деньги на медосмотр"};
  q.outcomes.clear();
  q.outcomes.push_back({2, 0, 2, 2});
  q.outcomes.push_back({10, -5, 10, 5});
  q.outcomes.push_back({-5, -15, -5, 10});
  roleQuestions["3"].push_back(q);

  q.text = "Вы очень сильно волнуетесь из-за того, где будут проходить роды, ваши "
           "действия";
  q.options = {"Промолчу, буду рожать в гос.клинике",
               "Обсужу с мужем, примем решение вместе",
               "Потребую денег на роды в платной клинике"};
  q.outcomes.clear();
  q.outcomes.push_back({-5, 0, -5, 2});
  q.outcomes.push_back({10, -5, 10, 5});
  q.outcomes.push_back({-10, -15, -10, 10});
  roleQuestions["3"].push_back(q);

  q.text = "У вас начались предродовые схватки ночью, что вы будете делать?";
  q.options = {"Разбужу мужа чтобы скорее поехать в больницу",
               "Позвоню в больницу", "Вызову такси"};
  q.outcomes.clear();
  q.outcomes.push_back({0, 0, 0, 2});
  q.outcomes.push_back({0, 0, -5, 5});
  q.outcomes.push_back({-5, -5, -5, 10});
  roleQuestions["3"].push_back(q);

  q.text = "Вы узнали о появлении нового члена семьи, ваши действия?";
  q.options = {"Давно хотел братика", "Начать психовать", "Сбежать из дома"};
  q.outcomes.clear();
  q.outcomes.push_back({5, 0, 5, 2});
  q.outcomes.push_back({-5, 0, -10, 5});
  q.outcomes.push_back({-10, -5, -15, 10});
  roleQuestions["4"].push_back(q);

  q.text = "Ваши друзья предлагают вам покурить и выпить";
  q.options = {"Соглашусь", "Откажусь", "Сдам их учителю"};
  q.outcomes.clear();
  q.outcomes.push_back({-10, -10, -10, 10});
  q.outcomes.push_back({5, 0, 5, 5});
  q.outcomes.push_back({0, 0, 0, 2});
  roleQuestions["4"].push_back(q);

  q.text = "Ты попросил у папы денег на компьютер, но он отказал";
  q.options = {"Найти сомнительную работу", "Пойти раздавать листовки",
               "Начать специально бесить родителей пока не купят"};
  q.outcomes.clear();
  q.outcomes.push_back({-5, -20, -10, 10});
  q.outcomes.push_back({5, 5, 5, 5});
  q.outcomes.push_back({-5, -10, -5, 2});
  roleQuestions["4"].push_back(q);

  q.text = "Кажется, на вас давно не обращали внимания";
  q.options = {"Побегать по квартире", "Мяукать пока вас не заметят",
               "Погрызть ноутбук"};
  q.outcomes.clear();
  q.outcomes.push_back({0, 0, 0, 2});
  q.outcomes.push_back({5, 0, 5, 5});
  q.outcomes.push_back({-5, -10, 0, 10});
  roleQuestions["5"].push_back(q);

  q.text = "Семья купила новую коляску для ребенка";
  q.options = {"Не обращать внимания", "Залезть в неё и начать царапать",
               "Сходить в неё в туалет"};
  q.outcomes.clear();
  q.outcomes.push_back({0, 0, 0, 2});
  q.outcomes.push_back({-5, -10, -5, 5});
  q.outcomes.push_back({-10, -5, -10, 10});
  roleQuestions["5"].push_back(q);

  q.text = "Все куда-то уехали";
  q.options = {"Поспать", "Что-нибудь разбить", "Попробовать сбежать"};
  q.outcomes.clear();
  q.outcomes.push_back({0, 0, 0, 5});
  q.outcomes.push_back({0, -5, -5, 2});
  q.outcomes.push_back({-10, -5, -10, 10});
  roleQuestions["5"].push_back(q);
}

void startGame() {
  broadcastMessage("Сценарий выбран: День рождение ребенка.");
  initQuestions();
  for (int round = 1; round <= MAX_ROUNDS; round++) {
    pthread_mutex_lock(&mutx);
    roundCnt = round;
    pthread_mutex_unlock(&mutx);
    broadcastMessage("Начинается раунд " + std::to_string(round) + ".");
    pthread_mutex_lock(&mutx);
    currentAnswers.clear();
    questionActive = true;
    pthread_mutex_unlock(&mutx);
    pthread_mutex_lock(&mutx);
    for (auto &entry : playerRoles) {
      int playerId = entry.first;
      std::string role = entry.second;
      if (roleQuestions.find(role) != roleQuestions.end() &&
          !roleQuestions[role].empty()) {
        int qIndex = (roundCnt - 1 < roleQuestions[role].size())
                         ? roundCnt - 1
                         : roleQuestions[role].size() - 1;
        Question q = roleQuestions[role][qIndex];
        std::ostringstream oss;
        oss << "Вопрос для игрока " << playerId << ": " << q.text << "\n";
        for (size_t i = 0; i < q.options.size(); i++) {
          oss << (i + 1) << ") " << q.options[i] << "\n";
        }
        int sock = playerSockets[playerId];
        sendMessage(sock, oss.str());
      }
    }
    pthread_mutex_unlock(&mutx);
    sleep(60);
    pthread_mutex_lock(&mutx);
    questionActive = false;
    for (auto &entry : playerRoles) {
      int playerId = entry.first;
      std::string role = entry.second;
      if (roleQuestions.find(role) != roleQuestions.end() &&
          !roleQuestions[role].empty()) {
        int qIndex = (roundCnt - 1 < roleQuestions[role].size())
                         ? roundCnt - 1
                         : roleQuestions[role].size() - 1;
        Question q = roleQuestions[role][qIndex];
        int answer = (currentAnswers.find(playerId) != currentAnswers.end())
                         ? currentAnswers[playerId]
                         : 1;
        Outcome out = q.outcomes[answer - 1];
        familyLove += out.loveChange;
        familyMoney += out.moneyChange;
        familyConnection += out.connectChange;
        playerScores[playerId] += out.scoreChange;
      }
    }
    pthread_mutex_unlock(&mutx);
    broadcastGameState();
    {
      std::ostringstream oss;
      oss << "TABLE:";
      bool first = true;
      pthread_mutex_lock(&mutx);
      for (auto &entry : playerScores) {
        if (!first)
          oss << ",";
        oss << entry.first << ":" << entry.second;
        first = false;
      }
      pthread_mutex_unlock(&mutx);
      broadcastMessage(oss.str());
    }
    if (familyMoney <= 0 || familyLove <= 0 || familyConnection <= 0) {
      broadcastMessage("Один из параметров достиг 0. Побеждает Жизнь!");
      return;
    }
  }
  int winnerId = -1, maxScore = -1;
  pthread_mutex_lock(&mutx);
  for (auto &entry : playerScores) {
    if (entry.second > maxScore) {
      maxScore = entry.second;
      winnerId = entry.first;
    }
  }
  pthread_mutex_unlock(&mutx);
  broadcastMessage("Игра завершена. Победитель из семьи: игрок " +
                   std::to_string(winnerId));
}

void *clientThread(void *arg) {
  ClientData *data = static_cast<ClientData *>(arg);
  int client = data->client;
  int playerId = data->playerId;
  std::cout << "Клиент " << playerId << " подключён.\n";
  pthread_mutex_lock(&mutx);
  playerScores[playerId] = 0;
  playerSockets[playerId] = client;
  pthread_mutex_unlock(&mutx);
  std::string welcome = "Добро пожаловать, игрок " + std::to_string(playerId);
  sendMessage(client, welcome);
  std::string instruction =
      "Для того чтобы начать игру, все игроки должны быть готовы.\n"
      "Выберите свою роль (введите число от 1 до 5):\n"
      "1) Жизнь - контролирует игровой процесс\n"
      "2) Муж\n"
      "3) Жена\n"
      "4) Ребёнок\n"
      "5) Кот/кошка";
  sendMessage(client, instruction);
  char buffer[BUFFER_SIZE];
  while (true) {
    memset(buffer, 0, sizeof(buffer));
    int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
      std::cout << "Игрок " << playerId << " отключился.\n";
      break;
    }
    std::string msg(buffer);
    std::cout << "Игрок " << playerId << " ввёл: " << msg << "\n";
    pthread_mutex_lock(&mutx);
    bool localRolesLocked = rolesLocked;
    bool localQuestionActive = questionActive;
    pthread_mutex_unlock(&mutx);
    if (!localRolesLocked) {
      if (msg == "1" || msg == "2" || msg == "3" || msg == "4" || msg == "5") {
        pthread_mutex_lock(&mutx);
        bool roleTaken = false;
        for (const auto &entry : playerRoles) {
          if (entry.second == msg && entry.first != playerId) {
            roleTaken = true;
            break;
          }
        }
        if (roleTaken) {
          pthread_mutex_unlock(&mutx);
          sendMessage(client,
                      "Ошибка: выбранная роль уже занята. Попробуйте другую.");
          continue;
        }
        playerRoles[playerId] = msg;
        pthread_mutex_unlock(&mutx);
        sendMessage(client, "Роль изменена на: " + msg);
        std::string readyMessage =
            "Игрок " + std::to_string(playerId) + " выбрал роль " + msg;
        broadcastMessage(readyMessage);
        pthread_mutex_lock(&mutx);
        if (playerRoles.size() == MAX_PLAYERS) {
          rolesLocked = true;
          pthread_mutex_unlock(&mutx);
          broadcastMessage("Все игроки выбрали роли. Игра начинается!");
          pthread_t gameThread;
          if (pthread_create(
                  &gameThread, nullptr,
                  [](void *) -> void * {
                    startGame();
                    return nullptr;
                  },
                  nullptr) != 0) {
            std::cerr << "Не удалось создать поток для игры.\n";
          }
        } else {
          pthread_mutex_unlock(&mutx);
        }
      } else {
        sendMessage(client, "Неверный ввод. Выберите роль: 1, 2, 3, 4 или 5.");
      }
      continue;
    }
    if (localQuestionActive) {
      int ans = std::atoi(buffer);
      if (ans < 1 || ans > 3) {
        sendMessage(client, "Неверный вариант ответа. Введите 1, 2 или 3.");
      } else {
        pthread_mutex_lock(&mutx);
        if (currentAnswers.find(playerId) == currentAnswers.end()) {
          currentAnswers[playerId] = ans;
          sendMessage(client, "Ответ принят: " + std::to_string(ans));
        }
        pthread_mutex_unlock(&mutx);
      }
    }
  }
  close(client);
  delete data;
  pthread_exit(nullptr);
  return nullptr;
}

int main() {
  int serverSock = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSock < 0) {
    perror("Ошибка: сокет не был создан");
    exit(1);
  }
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(SERVER_PORT);
  if (bind(serverSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
      0) {
    perror("Ошибка: сокет не был привязан");
    close(serverSock);
    exit(1);
  }
  if (listen(serverSock, MAX_PLAYERS) < 0) {
    perror("Ошибка: не удалось слушать");
    close(serverSock);
    exit(1);
  }
  pthread_mutex_init(&mutx, nullptr);
  std::cout << "Сервер запущен. Прослушивание порта " << SERVER_PORT << "\n";
  std::vector<pthread_t> threads;
  int playerCount = 0;
  while (playerCount < MAX_PLAYERS) {
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    int clientSock =
        accept(serverSock, (struct sockaddr *)&clientAddr, &addrLen);
    if (clientSock < 0) {
      perror("Ошибка: не удалось принять соединение с клиентом");
      sleep(3);
      continue;
    }
    playerCount++;
    pthread_mutex_lock(&mutx);
    clientSockets.push_back(clientSock);
    pthread_mutex_unlock(&mutx);
    ClientData *data = new ClientData;
    data->client = clientSock;
    data->playerId = playerCount;
    pthread_t thread;
    if (pthread_create(&thread, nullptr, clientThread, data) != 0) {
      std::cout << "Ошибка создания потока для игрока " << playerCount << "\n";
      delete data;
      close(clientSock);
    } else {
      threads.push_back(thread);
    }
  }
  for (auto thread : threads)
    pthread_join(thread, nullptr);
  std::cout << "Игра завершена. Сервер выключается.\n";
  close(serverSock);
  pthread_mutex_destroy(&mutx);
  return 0;
}
