#include "opendevice.h"

#include <termios.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <QIODevice>
#include <QtDebug>

#include "qserialport_android_p.h"
#include "private/qringbuffer_p.h"

#define TRUE 1
#define FALSE 0

speed_t speed_arr[] = {B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300, B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int name_arr[] = {115200, 38400, 19200, 9600, 4800, 2400, 1200, 300, 115200, 38400, 19200, 9600, 4800, 2400, 1200, 300, };

int set_Parity(int fd,int databits,int stopbits,int parity)
{
  struct termios options;
  if ( tcgetattr( fd,&options) != 0){
    qDebug() << "set params failed 1";
    return (FALSE);
  }

  bzero(&options,sizeof(options));
  options.c_cflag |= CLOCAL | CREAD;
  options.c_cflag &= ~CSIZE;
  switch (databits)
  {
    case 7:
        options.c_cflag |= CS7;
    break;
    case 8:
        options.c_cflag |= CS8;
    break;
    default:
        options.c_cflag |= CS8;
    break;
  }
  switch (parity)
  {
    case 0:       // N
        options.c_cflag &= ~PARENB;
        options.c_iflag &= ~INPCK;
    break;
    case 1:       // O
        options.c_cflag |= (PARODD | PARENB);
        options.c_iflag |= (INPCK | ISTRIP);
    break;
    case 2:       // E
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_iflag |= (INPCK | ISTRIP);
    break;
    case 3:       // S
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
    break;
    default:
        qDebug() << "Unsupported parity";
    return (FALSE);
  }
  switch (stopbits)
  {
    case 1:
        options.c_cflag &= ~CSTOPB;
    break;
    case 2:
        options.c_cflag |= CSTOPB;
    break;
    default: qDebug() << "Unsupported stop bits";
    return (FALSE);
    }
    if (parity != 0)   // N
        options.c_iflag |= INPCK;
    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 0;
    tcflush(fd,TCIFLUSH);
    if (tcsetattr(fd,TCSANOW, &options) != 0)
    {
        qDebug() << "set params failed 3";
        return (FALSE);
    }
    return (TRUE);
}

AndroidSerialPort::AndroidSerialPort(QSerialPortPrivate* pSerial, QObject *parent)
    :QThread(parent)
    ,serialPortPri(pSerial)
{

}

AndroidSerialPort::~AndroidSerialPort()
{

}

int AndroidSerialPort::openPort(const QString& name, quint32 mode)
{
    Q_UNUSED(mode)
    _portName = name;
    _deviceId = open(_portName.toLatin1().data(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(-1 == _deviceId) {
        qDebug() << "Can't Open Serial Port: " << _portName;
        return 0;
    }
    else{
        qDebug() << "Open com success!";
        if(setParameters(_deviceId, _baudRate, _dataBits, _stopBits, _parity)) {
            return _deviceId;
        } else {
            qDebug() << "set com parameter failed";
            closePort();
            return 0;
        }
    }
}

int AndroidSerialPort::closePort()
{
    if(_deviceId > 0) {
        int ret = close(_deviceId);    /// success 0  failed  -1
        if(!ret) {
            qDebug() << "Close com success!";
        }
        return ret;
    }
    return 0;
}

void AndroidSerialPort::startReadThread()
{
    if(_deviceId > 0) {
        start();
    }
}

void AndroidSerialPort::stopReadThread()
{
    _quitThread = true;
}

qint64 AndroidSerialPort::writeData(const char *buffer, qint64 len)
{
    return write(_deviceId, buffer, len);
}

bool AndroidSerialPort::setParameters(int deviceid, int baudRateA, int dataBitsA, int stopBitsA, int parityA)
{
    bool ret = set_Parity(deviceid, dataBitsA, stopBitsA, parityA);
    if(ret){
        return setBaudRate(deviceid, baudRateA);
    } else {
        return false;
    }
}

bool AndroidSerialPort::setBaudRate(int deviceid, qint32 baudRate)
{
    int status;
    struct termios Opt;
    tcgetattr(deviceid, &Opt);
    for (quint32 i= 0; i < sizeof(speed_arr) / sizeof(int); i++){
        if (baudRate == name_arr[i]){
            tcflush(deviceid, TCIOFLUSH);
            cfsetispeed(&Opt, speed_arr[i]);
            cfsetospeed(&Opt, speed_arr[i]);
            status = tcsetattr(deviceid, TCSANOW, &Opt);
            if (status != 0) {
                qDebug() << ("tcsetattr fd1");
            }
            return status == 0;
        }
        tcflush(deviceid,TCIOFLUSH);
    }
    return false;
}

void AndroidSerialPort::run()
{
    qDebug() << "AndroidSerialPort::run:" << _deviceId << _dataBits << _stopBits << _baudRate << _parity;
    if(_bufferSize <= 0) {
        _bufferSize = 2048;
    }
    while(!_quitThread) {
        readNotification();
        QThread::msleep(10);
    }
    qDebug() << "AndroidSerialPort::run exit";
}

bool AndroidSerialPort::readNotification()
{
    qint64 newBytes    = 0;
    qint64 bytesToRead = 32768;
//    QByteArray readBuffer;
    QRingBuffer readBuffer;
    if (_bufferSize && bytesToRead > (_bufferSize - readBuffer.size())) {
        bytesToRead = _bufferSize - readBuffer.size();
        if (bytesToRead <= 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
//            setReadNotificationEnabled(false);
            return false;
        }
    }

    char* ptr = readBuffer.reserve(bytesToRead);

    const qint64 readBytes = read(_deviceId, ptr, bytesToRead);

    if (readBytes <= 0) {
        return false;
    }

    readBuffer.chop(bytesToRead - qMax(readBytes, qint64(0)));

    newBytes = readBuffer.size() - newBytes;

    // only emit readyRead() when not recursing, and only if there is data available
    const bool hasData = newBytes > 0;
    if (hasData) {
        if(serialPortPri) {
            serialPortPri->newDataArrived(ptr, newBytes);
        }
    }
    return true;
}
