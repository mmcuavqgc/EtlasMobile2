#ifndef OPENDEVICE_H
#define OPENDEVICE_H

#include <math.h>
#include <QThread>
#include <QMutex>

#define MAX_BUFF_SIZE (2048)
class QSerialPortPrivate;

typedef struct _serial_parse
{
    char buff[MAX_BUFF_SIZE];
    int rxbuffsize;
}serial_parse;


class AndroidSerialPort : QThread {
    Q_OBJECT
public:
    explicit AndroidSerialPort(QSerialPortPrivate* pSerial, QObject* parent = nullptr);
    virtual ~AndroidSerialPort() override;

    int openPort(const QString& name, quint32 mode);
    int closePort();

    void startReadThread();
    void stopReadThread();

    qint64 writeData(const char* buffer, qint64 len);

    bool setParameters(int deviceid, int baudRateA, int dataBitsA, int stopBitsA, int parityA);
    bool setBaudRate(int deviceid, qint32 baudRate);

    void setBufferMaxSize(qint64 maxSize) { _bufferSize = maxSize; }
protected:
    void run() override;
private:
    bool readNotification();
private:
    QString    _portName;
    qint32     _baudRate    = 115200;
    qint32     _dataBits    = 8;
    qint32     _parity      = 0;
    qint32     _stopBits    = 1;
    qint32     _deviceId    = -1;
    qint64     _bufferSize  = MAX_BUFF_SIZE;
    bool       _quitThread  = false;
    bool       _emittedReadyRead = false;
    QSerialPortPrivate*     serialPortPri = nullptr;
    QMutex     _mutex;
};

int set_Parity(int fd,int databits,int stopbits,int parity);

#endif // OPENDEVICE_H
