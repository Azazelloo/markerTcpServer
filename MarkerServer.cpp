#include "MarkerServer.hpp"

MarkerServer::MarkerServer():sizePhotoFrame(0),sizePhotoFull(0),indexPhoto(0),photoRcv(false){
    server = new QTcpServer();
    if(server -> listen(QHostAddress("192.168.143.181"),45552))
        qDebug()<<"Listening adress 192.168.143.181 port 45552...";
    else
        qDebug()<<"Server error!";

    connect(server,&QTcpServer::newConnection,this,&MarkerServer::onNewConnection);

    //_____формируем общий заголовок json квитанции
    m_jsonMsg = new QJsonObject{
    {"name","Message"},
    {"id",0},
    {"ref_id",0},
    {"timestamp",""},
    {"sender",""},
    {"receiver",""}
};
}

void MarkerServer::onNewConnection(){
    qDebug()<<"New connection!";

    socket = server -> nextPendingConnection();
    connect(socket,&QTcpSocket::readyRead,this,&MarkerServer::onReadyRead);
}

int MarkerServer::writeFrame(QByteArray const & buffer){
    if(!m_file)
        reopenFile();

    return m_file -> write(buffer);
}

void MarkerServer::reopenFile(){
    m_file = new QFile("photo"+ QString::number(indexPhoto) +".jpg");
    if(!m_file -> open(QIODevice::WriteOnly))
        qDebug()<<"Error open *.jpg!";
}

void MarkerServer::closeFile(){
    if(m_file){
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void MarkerServer::getFrame(QByteArray&  buffer){
    int sizeInt = static_cast<int>(sizeof(int));
    //_____проверяем пришли ли 4 байта содержащие размер заголовка
    if(buffer.size() < sizeInt){
        tailBuffer = buffer;
        photoRcv = false;
        return;
    }

    //____получаем размер заголовка
    QDataStream inBuf(&buffer,QIODevice::ReadOnly);
    int sizeHeader = 0;
    inBuf >> sizeHeader;

    //______проверяем пришел ли полный заголовок
    if(buffer.size() < (sizeHeader + sizeInt)){
        tailBuffer = buffer;
        photoRcv = false;
        return;
    }

    //_____разбираем заголовок
    header = QJsonDocument::fromJson(QByteArray{buffer.data()+sizeof(sizeHeader),sizeHeader});
    auto headerArray = header.toJson(QJsonDocument::Indented);
    if(!header.isNull()){
        qDebug()<<headerArray.toStdString().c_str();
        sizePhotoFull = header["data"]["size"].toInt();

        buffer.remove(0,sizeof(sizeHeader) + sizeHeader);
        //_____доклеиваем фреймы
        if(buffer.size() < sizePhotoFull){
            sizePhotoFrame = buffer.size();
            writeFrame(buffer);
            photoRcv = true;
            return;
        }
        //_____последний фрейм + начало следующего заголовка
        else{
            int size = buffer.size() - sizePhotoFull;
            writeFrame(QByteArray{buffer.data(),sizePhotoFull});
            qDebug()<<"HEAD: >= INDEX PHOTO -> "<<++indexPhoto;
            closeFile();
            sendReceipt(header);

            sizePhotoFrame = 0;
            photoRcv = false;

            if(size){
                buffer.remove(0,sizePhotoFull);
                getFrame(buffer);
            }

        }
    }
}

void MarkerServer::sendReceipt(QJsonDocument& doc){
    //_____формируем квитанцию о доставке
    m_jsonMsg -> insert("id",m_receiptIndex++);
    m_jsonMsg -> insert("ref_id",doc["id"].toInt());
    m_jsonMsg -> insert("timestamp",QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzz"));
    m_jsonMsg -> insert("sender",doc["receiver"].toString());
    m_jsonMsg -> insert("receiver",doc["sender"].toString());

    QJsonObject jsonData{
        {"name","Receipt"},
        {"_comment","It is receipt!"}
    };

    m_jsonMsg->insert("date",jsonData);
    socket -> write(QJsonDocument(*m_jsonMsg).toJson());
}

void MarkerServer::onReadyRead(){
    //_____если заголовок в предыдущем фрейме был неполный
    if(tailBuffer.size()){
        buffer = tailBuffer;
        buffer.append(socket -> readAll());
        tailBuffer.clear();
    }
    else
        buffer = socket -> readAll();

    auto doc = QJsonDocument::fromJson(buffer);
    //_____все типы сообщений кроме фотографий
    if(!doc.isNull()){
        qDebug()<< doc.toJson(QJsonDocument::Indented).toStdString().c_str();
        sendReceipt(doc);
    }
    //_____фотография
    else{
        if(!photoRcv)
            getFrame(buffer);
        else{
            sizePhotoFrame+=buffer.size();
            //_____доклеиваем буфер
            if(sizePhotoFrame < sizePhotoFull){
                writeFrame(buffer);
            }
            //_____последний фрейм + начало следующего заголовка
            else{
                int size = sizePhotoFrame - sizePhotoFull;
                writeFrame(QByteArray{buffer.data(),buffer.size() - size});
                qDebug()<<">= INDEX PHOTO -> "<<++indexPhoto;
                closeFile();
                sendReceipt(header);                

                sizePhotoFull = 0;
                sizePhotoFrame = 0;
                photoRcv = false;

                if(size){
                    buffer.remove(0,buffer.size() - size);
                    getFrame(buffer);
                }
            }
        }
    }
}
