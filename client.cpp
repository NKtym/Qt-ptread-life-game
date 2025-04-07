#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <unistd.h>

#define SERVER_PORT 53000
#define BUFFER_SIZE 1024

class NetworkThread : public QThread {
  Q_OBJECT
public:
  int client;
  bool connected = false;
  void run(){
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0) {
      emit messageReceived("Ошибка: сокет не был создан.");
      return;
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
      emit messageReceived("Ошибка: неверный IP адрес.");
      close(client);
      return;
    }
    if (::connect(client, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) <
        0) {
      emit messageReceived("Ошибка: не удалось подключиться к серверу.");
      close(client);
      return;
    }
    connected = true;
    emit messageReceived("Подключение установлено.");
    char buffer[BUFFER_SIZE];
    while (connected) {
      memset(buffer, 0, sizeof(buffer));
      int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
      if (bytes <= 0) {
        connected = false;
        break;
      }
      std::string msg(buffer);
      emit messageReceived(QString::fromStdString(msg));
      msleep(100);
    }
    close(client);
  }
  bool sendMessage(const std::string &msg) {
    if (send(client, msg.c_str(), msg.size() + 1, 0) < 0)
      return false;
    return true;
  }
signals:
  void messageReceived(const QString &msg);
};

class ClientWidget : public QWidget {
  Q_OBJECT
public:
  ClientWidget(QWidget *parent = nullptr)
      : QWidget(parent), gameStarted(false) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    QHBoxLayout *topLayout = new QHBoxLayout();
    roleImageLabel = new QLabel(this);
    roleImageLabel->setText(
        "<img src='images/default.png' width='160' height='160'>");
    topLayout->addWidget(roleImageLabel, 0, Qt::AlignLeft | Qt::AlignTop);
    roundLabel = new QLabel("Раунд: -", this);
    topLayout->addWidget(roundLabel, 1, Qt::AlignCenter);
    layout->addLayout(topLayout);
    QHBoxLayout *parametersLayout = new QHBoxLayout();
    parametersLayout->setAlignment(Qt::AlignCenter);
    parametersLayout->setSpacing(120);
    moneyLabel = new QLabel(this);
    moneyLabel->setText(
        "<img src='images/money.png' width='30' height='30'> Деньги: -%");
    parametersLayout->addWidget(moneyLabel);
    loveLabel = new QLabel(this);
    loveLabel->setText(
        "<img src='images/love.png' width='35' height='35'> Любовь: -%");
    parametersLayout->addWidget(loveLabel);
    connectionLabel = new QLabel(this);
    connectionLabel->setText(
        "<img src='images/connection.png' width='37' height='37'> Связь: -%");
    parametersLayout->addWidget(connectionLabel);
    layout->addLayout(parametersLayout);
    scoreTable = new QTableWidget(this);
    scoreTable->setColumnCount(2);
    scoreTable->setHorizontalHeaderLabels(QStringList() << "Игрок"
                                                        << "Очки");
    scoreTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(scoreTable);
    log = new QTextEdit(this);
    log->setReadOnly(true);
    layout->addWidget(log);
    QHBoxLayout *inputLayout = new QHBoxLayout();
    input = new QLineEdit(this);
    QPushButton *sendButton = new QPushButton("Отправить", this);
    inputLayout->addWidget(input);
    inputLayout->addWidget(sendButton);
    layout->addLayout(inputLayout);
    networkThread = new NetworkThread();
    connect(networkThread, &NetworkThread::messageReceived, this,
            &ClientWidget::onMessageReceived);
    networkThread->start();
    connect(sendButton, &QPushButton::clicked, this,
            &ClientWidget::sendMessage);
    connect(input, &QLineEdit::returnPressed, this, &ClientWidget::sendMessage);
  }

  ~ClientWidget() {
    networkThread->quit();
    networkThread->wait();
    delete networkThread;
  }
public slots:
  void onMessageReceived(const QString &msg) {
    if (msg.startsWith("TABLE:")) {
      updateScoreTable(msg);
      return;
    }
    if (msg.contains("Все игроки выбрали роли. Игра начинается!"))
      gameStarted = true;
    if (msg.contains("Начинается раунд 1.")) {
      roundLabel->setText("Раунд: 1");
      moneyLabel->setText(
          "<img src='images/money.png' width='30' height='30'> " +
          QString::fromStdString("Деньги: ") + QString::number(100) + "%");
      loveLabel->setText("<img src='images/love.png' width='35' height='35'> " +
                         QString::fromStdString("Любовь: ") +
                         QString::number(100) + "%");
      connectionLabel->setText(
          "<img src='images/connection.png' width='37' height='37'> " +
          QString::fromStdString("Связь: ") + QString::number(100) + "%");
    }
    if (msg.startsWith("Раунд:")) {
      QStringList parts = msg.split('|');
      QString roundStr = parts[0].trimmed();
      roundStr.remove("Раунд:");
      int roundNum = roundStr.trimmed().toInt();
      roundNum++;
      if (parts.size() >= 4) {
        roundLabel->setText("Раунд: " + QString::number(roundNum));
        moneyLabel->setText(
            "<img src='images/money.png' width='30' height='30'> " +
            parts[1].trimmed());
        loveLabel->setText(
            "<img src='images/love.png' width='35' height='35'> " +
            parts[2].trimmed());
        connectionLabel->setText(
            "<img src='images/connection.png' width='37' height='37'> " +
            parts[3].trimmed());
      }
    }
    log->append(msg);
  }
  void sendMessage() {
    QString text = input->text();
    if (!text.isEmpty()) {
      if (!gameStarted && (text == "1" || text == "2" || text == "3" ||
                           text == "4" || text == "5")) {
        updateRoleIcon(text);
      }
      if (!networkThread->sendMessage(text.toStdString()))
        log->append("Не удалось отправить сообщение.");
      input->clear();
    }
  }

private:
  QLabel *roundLabel;
  QLabel *moneyLabel;
  QLabel *loveLabel;
  QLabel *connectionLabel;
  QLabel *roleImageLabel;
  QTableWidget *scoreTable;
  QTextEdit *log;
  QLineEdit *input;
  NetworkThread *networkThread;
  bool gameStarted;
  void updateRoleIcon(const QString &role) {
    QString iconPath;
    if (role == "1")
      iconPath = "images/life.png";
    else if (role == "2")
      iconPath = "images/dad.png";
    else if (role == "3")
      iconPath = "images/mam.png";
    else if (role == "4")
      iconPath = "images/child.png";
    else if (role == "5")
      iconPath = "images/cat.jpg";
    else
      iconPath = "images/default.png";
    roleImageLabel->setText("<img src='" + iconPath +
                            "' width='160' height='160'>");
  }
  void updateScoreTable(const QString &tableMsg) {
    QString data = tableMsg.mid(6);
    QStringList entries = data.split(',', Qt::SkipEmptyParts);
    scoreTable->setRowCount(entries.size() - 1);
    int row = 0;
    for (const QString &entry : entries) {
      QStringList pair = entry.split(':');
      if (pair.size() == 2 && pair[0].trimmed() != "1") {
        QTableWidgetItem *playerItem = new QTableWidgetItem(pair[0].trimmed());
        QTableWidgetItem *scoreItem = new QTableWidgetItem(pair[1].trimmed());
        scoreTable->setItem(row, 0, playerItem);
        scoreTable->setItem(row, 1, scoreItem);
        row++;
      }
    }
  }
};

#include "client.moc"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  ClientWidget widget;
  widget.setWindowTitle("Клиент текстового квеста");
  widget.resize(1000, 1000);
  widget.show();
  return app.exec();
}