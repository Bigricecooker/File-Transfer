#ifndef Client_H
#define Client_H

#include <QtWidgets/QMainWindow>
#include "ui_Client.h"
#include <QLineEdit>
#include <QPushButton>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")

class Client : public QMainWindow
{
    Q_OBJECT

signals:
    void emitLoginResult(const QString& message);
    void loginSuccess(SOCKET sock);

public:
    Client(QWidget *parent = nullptr);
    ~Client();

	/// <summary>
	/// 登录函数，执行登录逻辑
	/// </summary>
	/// <param name="username">用户名</param>
	/// <param name="password">密码</param>
	void Login(const QString& username, const QString& password);

private:
    Ui::ClientClass ui;

    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_loginButton;

    SOCKET m_sock;
	std::mutex m_mtx;
    
private slots:
    /// <summary>
    /// 登录按钮点击事件处理函数
    /// </summary>
    void onLoginClicked();
	/// <summary>
	/// 登录失败处理函数
	/// </summary>
	/// <param name="message">失败消息</param>
	void onLoginFailed(const QString& message);
	/// <summary>
	/// 登录成功处理函数
	/// </summary>
	/// <param name="sock">成功的套接字</param>
	void onLoginSuccess(SOCKET sock);
};


#endif 