#include "MainWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QStringList>
#include <QMessageBox>
#include "const.h"

MainWidget::MainWidget(SOCKET sock, QWidget *parent)
    : QWidget(parent), m_sock(sock)
{
    setupUI();

    setWindowTitle("文件传输主界面");
    setFixedSize(550, 450);
}

MainWidget::~MainWidget()
{
    if (m_workThread.joinable())
        m_workThread.join();
}

void MainWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    this->setStyleSheet(
        "QWidget { background-color: #f2f5f9; font-family: 'Microsoft YaHei'; }"
        "QListWidget { background-color: #ffffff; border: 1px solid #dcdfe6; border-radius: 4px; padding: 5px; font-size: 14px; color: #606266; outline: none; }"
        "QListWidget::item { height: 35px; border-bottom: 1px solid #ebeef5; }"
        "QListWidget::item:hover { background-color: #f5f7fa; }"
        "QListWidget::item:selected { background-color: #ecf5ff; color: #409eff; font-weight: bold; border-left: 3px solid #409eff; }"
        "QPushButton { background-color: #409eff; color: white; border: none; border-radius: 4px; padding: 8px 15px; font-size: 14px; font-weight: bold; min-width: 80px; }"
        "QPushButton:hover { background-color: #66b1ff; }"
        "QPushButton:pressed { background-color: #3a8ee6; }"
        "QLabel { color: #909399; font-size: 13px; }"
    );

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    mainLayout->addWidget(m_listWidget);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);
    m_btnRefresh = new QPushButton("刷新列表", this);
    m_btnUpload = new QPushButton("上传文件", this);
    m_btnDownload = new QPushButton("下载文件", this);

    m_btnRefresh->setStyleSheet("background-color: #e6a23c;");
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnUpload->setStyleSheet("background-color: #67c23a;");
    m_btnUpload->setCursor(Qt::PointingHandCursor);
    m_btnDownload->setCursor(Qt::PointingHandCursor);

    btnLayout->addWidget(m_btnRefresh);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnUpload);
    btnLayout->addWidget(m_btnDownload);
    mainLayout->addLayout(btnLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 10000);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("%p%");
    m_progressBar->setFixedHeight(22);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: 1px solid #dcdfe6; border-radius: 4px; background: #f5f7fa; text-align: center; font-size: 12px; color: #606266; }"
        "QProgressBar::chunk { background-color: #409eff; border-radius: 3px; }"
    );
    m_progressBar->hide();
    mainLayout->addWidget(m_progressBar);

    m_lblStatus = new QLabel("欢迎使用文件传输客户端", this);
    mainLayout->addWidget(m_lblStatus);

    connect(m_btnUpload, &QPushButton::clicked, this, &MainWidget::onUploadClicked);
    connect(m_btnDownload, &QPushButton::clicked, this, &MainWidget::onDownloadClicked);
    connect(m_btnRefresh, &QPushButton::clicked, this, &MainWidget::onRefreshClicked);

    connect(this, &MainWidget::uploadProgress, this, &MainWidget::onUploadProgress);
    connect(this, &MainWidget::uploadFinished, this, &MainWidget::onUploadFinished);
    connect(this, &MainWidget::downloadProgress, this, &MainWidget::onDownloadProgress);
    connect(this, &MainWidget::downloadFinished, this, &MainWidget::onDownloadFinished);
    connect(this, &MainWidget::refreshFinished, this, &MainWidget::onRefreshFinished);
    connect(this, &MainWidget::setStatus, this, &MainWidget::onSetStatus);
}

void MainWidget::setButtonsEnabled(bool enabled)
{
    m_btnUpload->setEnabled(enabled);
    m_btnDownload->setEnabled(enabled);
    m_btnRefresh->setEnabled(enabled);
}

void MainWidget::onUploadProgress(quint64 sent, quint64 total)
{
    m_progressBar->setRange(0, 10000);
    m_progressBar->setValue(static_cast<int>(sent * 10000 / total));
    m_progressBar->show();
    m_lblStatus->setText(QString("上传中: %1 / %2 字节").arg(sent).arg(total));
}

void MainWidget::onUploadFinished(bool success, const QString& msg)
{
    m_progressBar->hide();
    setButtonsEnabled(true);
    m_lblStatus->setText(msg);
}

void MainWidget::onDownloadProgress(quint64 received, quint64 total)
{
    m_progressBar->setRange(0, 10000);
    m_progressBar->setValue(static_cast<int>(received * 10000 / total));
    m_progressBar->show();
    m_lblStatus->setText(QString("下载中: %1 / %2 字节").arg(received).arg(total));
}

void MainWidget::onDownloadFinished(bool success, const QString& msg)
{
    m_progressBar->hide();
    setButtonsEnabled(true);
    m_lblStatus->setText(msg);
}

void MainWidget::onRefreshFinished(const QStringList& files)
{
    m_listWidget->clear();
    for (const auto& f : files)
        m_listWidget->addItem(f);
    setButtonsEnabled(true);
}

void MainWidget::onSetStatus(const QString& msg)
{
    m_lblStatus->setText(msg);
}

void MainWidget::onUploadClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要上传的文件");
    if (filePath.isEmpty()) return;

    if (m_workThread.joinable())
        m_workThread.join();

    setButtonsEnabled(false);
    m_lblStatus->setText("准备上传文件...");

    m_workThread = std::thread([this, filePath]() {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit uploadFinished(false, "无法打开文件");
            return;
        }

        uint64_t totalSize = file.size();
        QString fileName = QFileInfo(filePath).fileName();
        QByteArray fileNameUtf8 = fileName.toUtf8();

        QByteArray fileData = file.readAll();
        uint32_t crc32 = calculateCRC32(fileData);
        file.seek(0);

        {
            std::lock_guard<std::mutex> lock(m_ioMtx);

            UploadReq req = {};
            strncpy_s(req.fileName, sizeof(req.fileName), fileNameUtf8.constData(), sizeof(req.fileName) - 1);
            req.fileSize = htonll(totalSize);
            req.crc32 = htonl(crc32);

            if (!sendPacket(m_sock, MSG_UPLOAD_REQ, &req, sizeof(req))) {
                emit uploadFinished(false, "发送上传请求失败");
                return;
            }

            uint16_t id;
            std::vector<char> data;
            if (!recvPacket(m_sock, id, data, m_recvBuffer) || id != MSG_RESP_OK) {
                emit uploadFinished(false, "服务器拒绝上传请求");
                return;
            }

        uint64_t sentSize = 0;
        uint32_t seq = 0;

        while (!file.atEnd()) {
            QByteArray chunk = file.read(chunkSize);
            if (chunk.isEmpty()) break;

            size_t dataLen = chunk.size();
            std::vector<uint8_t> buffer(sizeof(UploadDataHeader) + dataLen);

            UploadDataHeader hdr;
            hdr.seq = htonl(seq);
            memcpy(buffer.data(), &hdr, sizeof(hdr));
            memcpy(buffer.data() + sizeof(hdr), chunk.constData(), dataLen);

            if (!sendPacket(m_sock, MSG_UPLOAD_DATA, buffer.data(), buffer.size())) {
                emit uploadFinished(false, "发送文件数据失败");
                return;
            }

            sentSize += dataLen;
            emit uploadProgress(static_cast<quint64>(sentSize), static_cast<quint64>(totalSize));
            ++seq;
        }
        }

        file.close();
        emit uploadFinished(true, "上传完成");
    });
}

void MainWidget::onDownloadClicked()
{
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "提示", "请在上方列表中选择要下载的文件！");
        return;
    }

    QString fileName = item->text();
    QString savePath = QFileDialog::getSaveFileName(this, "保存文件到", fileName);
    if (savePath.isEmpty()) return;

    if (m_workThread.joinable())
        m_workThread.join();

    setButtonsEnabled(false);
    m_lblStatus->setText(QString("准备下载文件: %1").arg(fileName));

    QByteArray fileNameUtf8 = fileName.toUtf8();

    m_workThread = std::thread([this, savePath, fileNameUtf8]() {
        std::lock_guard<std::mutex> lock(m_ioMtx);

        DownloadReq req = {};
        strncpy_s(req.fileName, sizeof(req.fileName), fileNameUtf8.constData(), sizeof(req.fileName) - 1);

        if (!sendPacket(m_sock, MSG_DOWNLOAD_REQ, &req, sizeof(req))) {
            emit downloadFinished(false, "发送下载请求失败");
            return;
        }

        uint16_t id;
        std::vector<char> data;
        if (!recvPacket(m_sock, id, data, m_recvBuffer) || id != MSG_RESP_OK) {
            emit downloadFinished(false, "服务器拒绝下载请求");
            return;
        }

        if (data.size() < sizeof(DownloadRsp)) {
            emit downloadFinished(false, "服务器返回数据异常");
            return;
        }

        DownloadRsp rsp;
        memcpy(&rsp, data.data(), sizeof(rsp));
        uint64_t fileSize = ntohll(rsp.fileSize);

        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit downloadFinished(false, "无法创建文件");
            return;
        }

        if (!file.resize(fileSize)) {
            file.close();
            emit downloadFinished(false, "文件预分配失败");
            return;
        }

        uint64_t totalReceived = 0;
        while (totalReceived < fileSize) {
            uint16_t chunkId;
            std::vector<char> chunkData;
            if (!recvPacket(m_sock, chunkId, chunkData, m_recvBuffer)) {
                file.close();
                emit downloadFinished(false, "接收数据失败");
                return;
            }

            if (chunkId != MSG_DOWNLOAD_DATA) {
                file.close();
                emit downloadFinished(false, "数据包类型错误");
                return;
            }

            if (chunkData.size() < sizeof(UploadDataHeader)) {
                file.close();
                emit downloadFinished(false, "数据包格式错误");
                return;
            }

            UploadDataHeader hdr;
            memcpy(&hdr, chunkData.data(), sizeof(hdr));
            uint32_t seq = ntohl(hdr.seq);

            const char* fileData = chunkData.data() + sizeof(UploadDataHeader);
            uint64_t fileDataSize = chunkData.size() - sizeof(UploadDataHeader);
            uint64_t offset = static_cast<uint64_t>(seq) * chunkSize;

            if (offset + fileDataSize > fileSize) {
                file.close();
                emit downloadFinished(false, "写入超出文件范围");
                return;
            }

            if (!file.seek(offset)) {
                file.close();
                emit downloadFinished(false, "文件定位失败");
                return;
            }

            if (file.write(fileData, static_cast<qint64>(fileDataSize)) != static_cast<qint64>(fileDataSize)) {
                file.close();
                emit downloadFinished(false, "写入文件失败");
                return;
            }

            totalReceived += fileDataSize;
            emit downloadProgress(static_cast<quint64>(totalReceived), static_cast<quint64>(fileSize));
        }

        file.close();
        emit downloadFinished(true, "下载完成");
    });
}

void MainWidget::onRefreshClicked()
{
    if (m_workThread.joinable())
        m_workThread.join();

    setButtonsEnabled(false);
    m_lblStatus->setText("正在请求文件列表...");

    m_workThread = std::thread([this]() {
        std::lock_guard<std::mutex> lock(m_ioMtx);

        if (!sendPacket(m_sock, MSG_GET_FILE_LIST, nullptr, 0)) {
            emit setStatus("请求文件列表失败");
            emit refreshFinished(QStringList());
            return;
        }

        uint16_t id;
        std::vector<char> data;
        if (!recvPacket(m_sock, id, data, m_recvBuffer) || id != MSG_FILE_LIST_RESP) {
            emit setStatus("服务器拒绝请求");
            emit refreshFinished(QStringList());
            return;
        }

        struct FileInfo {
            char fileName[256];
        };

        size_t count = data.size() / sizeof(FileInfo);
        const FileInfo* files = reinterpret_cast<const FileInfo*>(data.data());
        QStringList list;
        for (size_t i = 0; i < count; ++i)
            list.append(QString::fromUtf8(files[i].fileName));

        emit refreshFinished(list);
        emit setStatus(QString("共 %1 个文件").arg(count));
    });
}
