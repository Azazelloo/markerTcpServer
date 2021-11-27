#pragma once
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDataStream>
#include <QDateTime>

class MarkerServer: public QObject{
public:
    MarkerServer();

    Q_SLOT void onNewConnection();
    Q_SLOT void onReadyRead();

private:
    void getFrame(QByteArray&);
    int writeFrame(QByteArray const &);
    void reopenFile();
    void closeFile();
    void sendReceipt(QJsonDocument&);

    QTcpServer* server {nullptr};
    QTcpSocket* socket {nullptr};

    QJsonDocument header;
    QJsonObject* m_jsonMsg {nullptr};
    QFile* m_file {nullptr};
    QByteArray buffer;
    QByteArray tailBuffer;
    int sizePhotoFrame , sizePhotoFull , indexPhoto;
    bool photoRcv;
    int m_receiptIndex = 0;
};
