#ifndef MAINWIDGET_H
#define MAINWIDGET_H 

#include <QWidget>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <qfile.h>
#include <thread>
#include <mutex>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

class QPushButton;
class QListWidget;
class QLabel;
class QProgressBar;
class QStringList;

class MainWidget : public QWidget
{
	Q_OBJECT

signals:
	void uploadProgress(quint64 sent, quint64 total);// 上传进度信号
	void uploadFinished(bool success, const QString& msg);// 上传完成信号
	void downloadProgress(quint64 received, quint64 total);// 下载进度信号
	void downloadFinished(bool success, const QString& msg);// 下载完成信号
	void refreshFinished(const QStringList& files);// 刷新文件列表完成信号
	void setStatus(const QString& msg);// 设置状态消息信号

public:
	 MainWidget(SOCKET sock, QWidget* parent = nullptr);
	~MainWidget();

private slots:
	/// <summary>
	/// 上传按钮点击事件处理函数
	/// </summary>
	void onUploadClicked();
	/// <summary>
	/// 下载按钮点击事件处理函数
	/// </summary>
	void onDownloadClicked();
	/// <summary>
	/// 刷新按钮点击事件处理函数
	/// </summary>
	void onRefreshClicked();

	/// <summary>
	/// 上传进度更新槽函数
	/// </summary>
	/// <param name="sent">已发送的字节数</param>
	/// <param name="total">总字节数</param>
	void onUploadProgress(quint64 sent, quint64 total);
	/// <summary>
	/// 上传完成槽函数
	/// </summary>
	/// <param name="success">上传是否成功</param>
	/// <param name="msg">上传结果消息</param>
	void onUploadFinished(bool success, const QString& msg);
	/// <summary>
	/// 下载进度更新槽函数
	/// </summary>
	/// <param name="received">已接收的字节数</param>
	/// <param name="total">总字节数</param>
	void onDownloadProgress(quint64 received, quint64 total);
	/// <summary>
	/// 下载完成槽函数
	/// </summary>
	/// <param name="success">下载是否成功</param>
	/// <param name="msg">下载结果消息</param>
	void onDownloadFinished(bool success, const QString& msg);
	/// <summary>
	/// 刷新文件列表完成槽函数
	/// </summary>
	/// <param name="files">文件列表</param>
	void onRefreshFinished(const QStringList& files);
	/// <summary>
	/// 设置状态消息槽函数
	/// </summary>
	/// <param name="msg">状态消息</param>
	void onSetStatus(const QString& msg);

private:
	/// <summary>
	/// 初始化UI组件
	/// </summary>
	void setupUI();
	/// <summary>
	/// 设置按钮的启用状态
	/// </summary>
	/// <param name="enabled"></param>
	void setButtonsEnabled(bool enabled);

	SOCKET m_sock;

	// UI组件
	QPushButton* m_btnUpload;
	QPushButton* m_btnDownload;
	QPushButton* m_btnRefresh;
	QListWidget* m_listWidget;
	QProgressBar* m_progressBar;
	QLabel* m_lblStatus;

	std::thread m_workThread;
	std::mutex m_ioMtx;
	std::vector<char> m_recvBuffer;
};
#endif