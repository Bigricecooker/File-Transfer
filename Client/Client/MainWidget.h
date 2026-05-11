#pragma once

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
	void uploadProgress(quint64 sent, quint64 total);
	void uploadFinished(bool success, const QString& msg);
	void downloadProgress(quint64 received, quint64 total);
	void downloadFinished(bool success, const QString& msg);
	void refreshFinished(const QStringList& files);
	void setStatus(const QString& msg);

public:
	 MainWidget(SOCKET sock, QWidget* parent = nullptr);
	~MainWidget();

private slots:
	void onUploadClicked();
	void onDownloadClicked();
	void onRefreshClicked();

	void onUploadProgress(quint64 sent, quint64 total);
	void onUploadFinished(bool success, const QString& msg);
	void onDownloadProgress(quint64 received, quint64 total);
	void onDownloadFinished(bool success, const QString& msg);
	void onRefreshFinished(const QStringList& files);
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
