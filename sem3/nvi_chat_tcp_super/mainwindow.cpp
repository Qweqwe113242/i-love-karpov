#include <QtNetwork/QNetworkDatagram>
#include <QtNetwork/QNetworkInterface>
#include <QTime>

#include "mainwindow.h"
#include "ui_mainwindow.h"

/// Конструктор
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Палитра для полей и кнопки
    normPal = ui->lineEditPort->palette();
    errPal = normPal;
    errPal.setColor(QPalette::Text, QColor(248, 23, 62));
    errPal.setColor(QPalette::ButtonText, QColor(248, 23, 62));

    // По нажатию Enter в поле ввода сообщения, сообщение должно отправляться
    connect(ui->lineEditMessage, &QLineEdit::returnPressed, this, &MainWindow::on_pushButtonSend_clicked);

    ui->lineEditMessage->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->checkBoxSpam->setEnabled(false);
    ui->pushButtonConnectionList->setEnabled(false);

    // Запуск таймера для спама в чат
    spamTimer.setInterval(500);
    spamTimer.start();
    connect(&spamTimer, &QTimer::timeout, this, &MainWindow::onSpam);

    ui->ServerWidget->hide();
    ui->ClientWidget->hide();
}

/// Деструктор
MainWindow::~MainWindow()
{
    clearServer();
    clearSockets();

    delete ui;
}


/// Вывод айпи адресов компьютера
void MainWindow::on_pushButtonAddresses_clicked()
{
    // Получаем все адреса
    auto adrs = QNetworkInterface::allAddresses();
    logInfo("IP адреса компьютера:");
    for (int i = 0; i < adrs.count(); i++)
    {
        // Отсеиваем только адреса типа IPv4
        if (adrs[i].protocol() == QAbstractSocket::IPv4Protocol)
            logInfo(QString("%1) <b>%2</b>").arg(i + 1)
                    .arg(addressToString(adrs[i])));
    }
}


/// Выбор режима сервера
void MainWindow::on_radioButtonServer_clicked()
{
    ui->ServerWidget->show();
    ui->ClientWidget->hide();
}

/// Выбор режима клиента
void MainWindow::on_radioButtonClient_clicked()
{
    ui->ServerWidget->hide();
    ui->ClientWidget->show();
}


/// Отправка введённого сообщения
void MainWindow::on_pushButtonSend_clicked()
{
    sendMessage(ui->lineEditMessage->text(), "Вы", "8314C2");

    ui->lineEditMessage->clear();
    ui->lineEditMessage->setFocus();
}

/// Слот открытия порта сервера
void MainWindow::on_pushButtonBind_clicked()
{
    // Проверка введённного с формы порта
    bool okPort2;
    int localPort = ui->lineEditLocalPort->text().toInt(&okPort2);

    if (okPort2 && localPort > 0 && localPort < 0x10000)
    {
        ui->lineEditLocalPort->setPalette(normPal);
        ui->pushButtonBind->setPalette(normPal);

        // Для начала на всякий слйчай отключаемся от сокета, сервера
        clearSockets();
        clearServer();

        // Создаём TCP сервер и открываем порт
        server = new QTcpServer(this);
        connect(server, &QTcpServer::newConnection, this, &MainWindow::onTcpBinded);
        if (server->listen(QHostAddress::Any, quint16(localPort)))
        {
            logInfo("Прослушиваем порт <b>" + QString::number(localPort) + "</b>");
        }
        else
        {
            logError("Ошибка при открытии сервера");
        }
    }
    else
    {
        // Если порт неправильный, сообщаем об ошибке
        ui->pushButtonBind->setPalette(errPal);
        ui->lineEditLocalPort->setPalette(errPal);
        logError("Введён неправильный порт");
    }
}

/// Слот установки соединения
void MainWindow::on_pushButtonConnect_clicked()
{
    // Проверка введённного с формы порта
    bool okPort1;
    int port = ui->lineEditPort->text().toInt(&okPort1);

    if (okPort1 && port > 0 && port < 0x10000)
    {
        ui->lineEditPort->setPalette(normPal);
        ui->pushButtonConnect->setPalette(normPal);

        // Для начала на всякий слйчай отключаемся от сокета, сервера
        clearSockets();
        clearServer();

        // Создаём принимающий TCP сокет
        QTcpSocket *sendSocket = new QTcpSocket(this);
        connect(sendSocket, &QTcpSocket::connected, this, &MainWindow::onTcpConnect);
        connect(sendSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpSendDisconnect);
        connect(sendSocket, &QTcpSocket::readyRead, this, &MainWindow::onTcpReadyRead);
        // В версии Qt 5.15 добавили сигнал errorOccured, который является заменой ныне устаревшего сигнала error
#if QT_VERSION_MAJOR >= 5 && QT_VERSION_MINOR >= 15
        connect(sendSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onTcpSendError);
#else
        connect(sendSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &MainWindow::onTcpSendError);
#endif
        // Подключаемся к серверу
        sendSocket->connectToHost(ui->lineEditAddress->text(), quint16(port));

        // Добавляем новый сокет
        sockets.append(sendSocket);
    }
    else
    {
        // Если порт неправильный, сообщаем об ошибке
        ui->pushButtonConnect->setPalette(errPal);
        ui->lineEditPort->setPalette(errPal);
        logError("Введён неправильный порт");
    }
}


/// Слот, соединённый с сигналом сервера, создаёт новое принимающее соединение
void MainWindow::onTcpBinded()
{
    QTcpSocket *getSocket = server->nextPendingConnection();
    if (getSocket != nullptr)
    {
        connect(getSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpGetDisconnect);
        connect(getSocket, &QTcpSocket::readyRead, this, &MainWindow::onTcpReadyRead);

        logInfo(QString("К серверу подключился <b>%1</b>!")
                .arg(addressToString(getSocket->localAddress())));


        auto connectData = addressToBytes(getSocket->localAddress());
        connectData.prepend(char(2));
        int socketCount = sockets.length();
        for (int i = 0; i < socketCount; i++)
        {
            sockets[i]->write(connectData);
        }

        // Добавляем новый сокет
        sockets.append(getSocket);

        ui->lineEditMessage->setEnabled(true);
        ui->pushButtonSend->setEnabled(true);
        ui->checkBoxSpam->setEnabled(true);
        ui->checkBoxSpam->setChecked(false);

        updateConnectionsIndicator();
    }
}

/// Слот, соединенённый с сигналом удачного соединения отправляющего сокета
void MainWindow::onTcpConnect()
{
    logInfo(QString("Соединение с <b>%1</b> установлено!")
        .arg(addressToString(sockets[0]->localAddress())));

    ui->lineEditMessage->setEnabled(true);
    ui->pushButtonSend->setEnabled(true);
    ui->checkBoxSpam->setEnabled(true);

    updateConnectionsIndicator();
}

/// Слот, соединённый с сигналом дисконнекта отправляющего сокета
void MainWindow::onTcpSendDisconnect()
{
    logWarn("Вы отключились от сервера");

    ui->lineEditMessage->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->checkBoxSpam->setEnabled(false);
    ui->checkBoxSpam->setChecked(false);

    clearSockets();

    updateConnectionsIndicator();
}

/// Слот, соединённый с сигналом дисконнекта принимающего сокета
void MainWindow::onTcpGetDisconnect()
{
    auto getSocket = static_cast<QTcpSocket *>(sender());
    if (getSocket != nullptr)
    {
        logWarn(QString("Собеседник <b>%1</b> отключился от вас")
                .arg(addressToString(getSocket->localAddress())));

        auto disconnectData = addressToBytes(getSocket->localAddress());
        disconnectData.prepend(char(3));
        int socketCount = sockets.length();
        for (int i = 0; i < socketCount; i++)
        {
            if (sockets[i] != getSocket)
                sockets[i]->write(disconnectData);
        }

        // Удаляем сокет из вектора сокетов
        sockets.removeOne(getSocket);
        getSocket->deleteLater();

        if (sockets.length() == 0)
        {
            // Если больше нет активных сокетов, отключаем возможность отправлять сообщения
            ui->lineEditMessage->setEnabled(false);
            ui->pushButtonSend->setEnabled(false);
            ui->checkBoxSpam->setEnabled(false);
            ui->checkBoxSpam->setChecked(false);
        }
    }

    updateConnectionsIndicator();
}

/// Слот, соединённый с сигналом об ошибки отправляющего сокета
void MainWindow::onTcpSendError(QAbstractSocket::SocketError err)
{
    // Небольшая обработка "известных" ошибок сокета,
    // чтобы не вводить пользователя в заблуждение
    switch (err) {
    case QAbstractSocket::RemoteHostClosedError:
        logError("Соединение с сервером закрылось");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        logError("Соединение отклонено");
        break;
    case QAbstractSocket::HostNotFoundError:
        logError("Хост не найден");
        break;
    default:
        logError("Ошибка сокета отправки");
    }

    auto socket = static_cast<QTcpSocket *>(sender());
    if (socket != nullptr)
    {
        // При ошибке закрываем отправляющий сокет
        socket->disconnect();
        socket->deleteLater();
        sockets.removeOne(socket);
    }

    ui->lineEditMessage->setEnabled(false);
    ui->pushButtonSend->setEnabled(false);
    ui->checkBoxSpam->setEnabled(false);
    ui->checkBoxSpam->setChecked(false);

    updateConnectionsIndicator();
}

/// Слот, соединённый с сигналом приёма данных принимающего сокета
void MainWindow::onTcpReadyRead()
{
    auto socket = static_cast<QTcpSocket *>(sender());
    if (socket != nullptr)
    {
        auto data = socket->readAll();
        if (data.size() > 0)
        {
            // Чтение данных из сокета
            if (server == nullptr)
            {
                // Если клиент получает данные
                if (data.size() > 0)
                {
                    switch (data[0]) {
                    case char(0): { // Если сообщение было отправлено именно сервером
                        // Считывание текста
                        QString text = QString::fromUtf8(data.mid(1));

                        logMessage(text, "Сервер", "3BD737");
                        break;
                    }
                    case char(1): { // Если сообщение было отправлено кем-то из клиентов
                        // Считывание IP-адреса
                        auto addr = bytesToAddress(data.mid(1, 4));
                        // Считывание текста
                        QString text = QString::fromUtf8(data.mid(5));

                        logMessage(text, addressToString(addr), "C2145C");
                        break;
                    }
                    case char(2): { // Кто-то из клиентов подключился
                        // Считывание IP-адреса
                        auto addr = bytesToAddress(data.mid(1, 4));

                        logInfo(QString("<b>%1</b> подключился к беседе")
                                .arg(addressToString(addr)));
                        break;
                    }
                    case char(3): { // Кто-то из клиентов отключился
                        // Считывание IP-адреса
                        auto addr = bytesToAddress(data.mid(1, 4));

                        logInfo(QString("<b>%1</b> отключился от беседы")
                                .arg(addressToString(addr)));
                        break;
                    }
                    }
                }
            }
            else
            {
                // Если сервер получает данные
                auto text = QString::fromUtf8(data);

                logMessage(text, addressToString(socket->localAddress()), "C2145C");

                data.prepend(addressToBytes(socket->localAddress()));
                data.prepend(char(1));
                int socketCount = sockets.length();
                for (int i = 0; i < socketCount; i++)
                {
                    if (sockets[i] != socket)
                        sockets[i]->write(data);
                }
            }
        }
    }
}


/// Метод для очистки сокетов
void MainWindow::clearSockets()
{
    int length = sockets.length();
    for (int i = 0; i < length; i++)
    {
        auto socket = sockets[i];
        // Если сокет активный, то отключаемся от него
        if (socket->isOpen())
        {
            socket->close();
            socket->disconnect();
        }
        // Отправляем на удаление
        socket->deleteLater();
    }
    // Очищаем массив адресов на сокеты
    sockets.clear();

    updateConnectionsIndicator();
}

/// Метод для остановки о удаления сервера
void MainWindow::clearServer()
{
    if (server != nullptr)
    {
        // Отключаем сервер, если он активный
        if (server->isListening())
            server->close();
        // Отправляем на удаление
        server->deleteLater();
        server = nullptr;
    }
}


/// Метод для более удобной отправки сообщений
void MainWindow::sendMessage(QString message, QString author, QString clr)
{
    if (message.length() > 0)
    {
        // Отправка данных
        QByteArray data = message.toUtf8();

        // Если сервер отправляет сообщение
        if (server != nullptr)
            data.prepend(char(0));

        int length = sockets.length();
        for (int i = 0; i < length; i++)
        {
            auto socket = sockets[i];
            socket->write(data);
        }
        // Вывод сообщения в "консоль"
        logMessage(message, author, clr);
    }
}


/// Обновление кнопки с количеством активных подключений
void MainWindow::updateConnectionsIndicator()
{
    int socketsCount = sockets.length();
    ui->pushButtonConnectionList->setText(QString("Подключено: %1").arg(socketsCount));
    if (socketsCount > 0)
        ui->pushButtonConnectionList->setEnabled(true);
    else
        ui->pushButtonConnectionList->setEnabled(false);
}


/// Слот отображения списка подключений
void MainWindow::on_pushButtonConnectionList_clicked()
{
    int socketCount = sockets.length();
    logInfo(QString("Подключено к <b>%1</b> %2:").arg(socketCount)
            .arg(socketCount % 10 == 1 ? "сокету" : "сокетам"));
    // Вывод списка всех подключений
    for (int i = 0; i < socketCount; i++)
    {
        logInfo(QString("%1) <b>%2</b>").arg(i + 1)
                .arg(addressToString(sockets[i]->localAddress())));
    }
}


QString MainWindow::addressToString(QHostAddress address)
{
    return address.toString().split(':').last();
}

QByteArray MainWindow::addressToBytes(QHostAddress address)
{
    char *addrAsBytes = new char[4];
    quint32 addrIPv4 = address.toIPv4Address();
    memcpy(addrAsBytes, &addrIPv4, 4);
    QByteArray ba(addrAsBytes, 4);
    delete [] addrAsBytes;
    return ba;
}

QHostAddress MainWindow::bytesToAddress(QByteArray ba)
{
    quint32 addrInt;
    memcpy(&addrInt, ba.constData(), 4);
    return QHostAddress(addrInt);
}


/// Вывод "страшного" красного сообщения в "консоль"
void MainWindow::logError(QString text)
{
    log("<span style=\"color: #F8173E\">" + text + "</span>");
}

/// Вывод предупрежения в "консоль"
void MainWindow::logWarn(QString text)
{
    log("<span style=\"color: #FFC618\">" + text + "</span>");
}

/// Вывод "дополнительной" информации в "консоль"
void MainWindow::logInfo(QString text)
{
    log("<span style=\"color: #4682B4\">" + text + "</span>");
}

/// Вывод сообщения в "консоль"
void MainWindow::logMessage(QString content, QString author, QString clr)
{
    QString ctStr = QTime::currentTime().toString("hh:mm:ss");
    log(QString("<b>[<span style=\"color: #%1\">%2</span>]</b> >> %3  "
                "<i style=\"color: #707070; font-size: 8pt;\">(%4)</i>")
        .arg(clr, author, content, ctStr));
}

/// Вывод текста в "консоль", сокращение для ui->chatTextEdit->append
void MainWindow::log(QString text)
{
    if (ui != nullptr)
        ui->chatTextEdit->append(text);
}


/// Слот автоматической отправки сообщения
void MainWindow::onSpam()
{
    if (ui->checkBoxSpam->isChecked() &&
            ui->lineEditMessage->isEnabled())
    {
        QString str = "Hello World! Привет, мир!";

        // Вывод сообщение о спаме
        sendMessage(str, "СПАМ", "F8173E");
    }
}

/// Слот очистки чата
void MainWindow::on_pushButtonClear_clicked()
{
    ui->chatTextEdit->clear();
}
