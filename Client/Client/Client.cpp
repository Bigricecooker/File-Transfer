#include "Client.h"
#include "MainWidget.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <QFont>
#include "const.h"
#include <thread>
#include <vector>

Client::Client(QWidget* parent)
    : QMainWindow(parent), m_sock(INVALID_SOCKET)
{
    this->ui.setupUi(this);

    setMenuBar(nullptr);        
    setStatusBar(nullptr);      

    // 2. 设置窗口标题
    setWindowTitle("文件传输");

    // 3. 固定大小并居中
    setFixedSize(400, 300);
    QRect screenGeometry = QGuiApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    // 4. 创建中心窗口部件
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    // 设置界面的全局QSS样式
    this->setStyleSheet(
        "QWidget#centralWidget { background-color: #f2f5f9; }"
        "QLabel { color: #333333; font-family: 'Microsoft YaHei'; font-size: 14px; }"
        "QLineEdit { border: 1px solid #cccccc; border-radius: 4px; padding: 6px 10px; font-family: 'Microsoft YaHei'; font-size: 14px; background-color: white; selection-background-color: #409eff; }"
        "QLineEdit:focus { border: 1px solid #409eff; }"
        "QPushButton { background-color: #409eff; color: white; border: none; border-radius: 4px; font-family: 'Microsoft YaHei'; font-weight: bold; padding: 8px; }"
        "QPushButton:hover { background-color: #66b1ff; }"
        "QPushButton:pressed { background-color: #3a8ee6; }"
        "QPushButton:disabled { background-color: #a0cfff; }"
    );

    // 5. 主布局（垂直）
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(40, 30, 40, 30);
    mainLayout->setSpacing(15);

    // 标题标签
    QLabel* titleLabel = new QLabel(tr("文件传输助手"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #409eff; font-size: 24px; font-weight: bold; margin-bottom: 15px;");
    mainLayout->addWidget(titleLabel); 

    // 用户名行
    QHBoxLayout* userRow = new QHBoxLayout();
    QLabel* userLabel = new QLabel(tr("用户名:"), this);
    userLabel->setFixedWidth(60);
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(tr("请输入用户名"));
    userRow->addWidget(userLabel);
    userRow->addWidget(m_usernameEdit);
    mainLayout->addLayout(userRow);

    // 密码行
    QHBoxLayout* pwdRow = new QHBoxLayout();
    QLabel* pwdLabel = new QLabel(tr("密  码:"), this);
    pwdLabel->setFixedWidth(60);
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("请输入密码"));
    pwdRow->addWidget(pwdLabel);
    pwdRow->addWidget(m_passwordEdit);
    mainLayout->addLayout(pwdRow);

    mainLayout->addSpacing(10);

    // 登录按钮
    m_loginButton = new QPushButton(tr("登  录"), this);
    m_loginButton->setFixedHeight(40);
    QFont btnFont = m_loginButton->font();
    btnFont.setPointSize(12);
    m_loginButton->setFont(btnFont);
    mainLayout->addWidget(m_loginButton);

    // 弹性空间，让控件靠上显示
    mainLayout->addStretch();

    // 连接登录按钮信号
    connect(m_loginButton, &QPushButton::clicked, this, &Client::onLoginClicked);
    connect(this, &Client::emitLoginResult, this, &Client::onLoginFailed);
    connect(this, &Client::loginSuccess, this, &Client::onLoginSuccess);
}

Client::~Client()
{
}

void Client::Login(const QString& username, const QString& password)
{
    m_loginButton->setEnabled(false);

    std::string user = username.toStdString();
    std::string pass = password.toStdString();


    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        emit emitLoginResult(QStringLiteral("创建socket失败"));
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        emit emitLoginResult(QStringLiteral("连接服务器失败"));
        return;
    }

    LoginReq req;
    memset(&req, 0, sizeof(req));
    strncpy_s(req.user, sizeof(req.user), user.c_str(), _TRUNCATE);
    strncpy_s(req.pwd, sizeof(req.pwd), pass.c_str(), _TRUNCATE);

    if (!sendPacket(sock, MSG_LOGIN, &req, sizeof(req))) {
        closesocket(sock);
        emit emitLoginResult(QStringLiteral("发送登录请求失败"));
        return;
    }

    uint16_t id;
    std::vector<char> data;
    std::vector<char> recvBuf;
    if (!recvPacket(sock, id, data, recvBuf)) {
        closesocket(sock);
        emit emitLoginResult(QStringLiteral("接收响应失败"));
        return;
    }

    if (id == MSG_RESP_OK) {
        std::lock_guard<std::mutex> lock(m_mtx);
        if (m_sock != INVALID_SOCKET)
            closesocket(m_sock);
        m_sock = sock;


        emit loginSuccess(sock);
    }
    else {
        closesocket(sock);
        emit emitLoginResult(QStringLiteral("登录失败，请检查用户名和密码！"));
    }
}

void Client::onLoginFailed(const QString& message)
{
    m_loginButton->setEnabled(true);
    QMessageBox::critical(this, tr("错误"), message);
}

void Client::onLoginSuccess(SOCKET sock)
{
    MainWidget* mainWidget = new MainWidget(sock);
    mainWidget->show();
    this->close();
}

void Client::onLoginClicked()
{
    QString username = m_usernameEdit->text();
    QString password = m_passwordEdit->text();

    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("警告"),
            tr("用户名不能为空！"));
        return;
    }
    if (password.isEmpty()) {
        QMessageBox::warning(this, tr("警告"),
            tr("密码不能为空！ "));
        return;
    }

    Login(username, password);
}