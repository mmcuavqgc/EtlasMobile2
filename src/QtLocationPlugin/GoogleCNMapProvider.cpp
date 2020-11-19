/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "GoogleCNMapProvider.h"
#if defined(DEBUG_GOOGLE_MAPS)
#include <QFile>
#include <QStandardPaths>
#endif
#include <QtGlobal>

#include "QGCMapEngine.h"

GoogleCNMapProvider::GoogleCNMapProvider(const QString &imageFormat, const quint32 averageSize, const QGeoMapType::MapStyle mapType, QObject* parent)
    : MapProvider(QStringLiteral("https://www.google.cn/maps/preview"), imageFormat, averageSize, mapType, parent)
    , _googleVersionRetrieved(false)
    , _googleReply(nullptr)
{
    _versionGoogleMap       = QStringLiteral("m@218");
    _versionGoogleSatellite = QStringLiteral("s@165");
    _versionGoogleLabels    = QStringLiteral("h@336");
    _versionGoogleTerrain   = QStringLiteral("t@131,r@218");
    _versionGoogleHybrid    = QStringLiteral("y");
    _secGoogleWord          = QStringLiteral("Galileo");

}

GoogleCNMapProvider::~GoogleCNMapProvider() {
    if (_googleReply)
        _googleReply->deleteLater();
}

//-----------------------------------------------------------------------------
void GoogleCNMapProvider::_getSecGoogleWords(const int x, const int y, QString& sec1, QString& sec2) const {
    sec1       = QStringLiteral(""); // after &x=...
    sec2       = QStringLiteral(""); // after &zoom=...
    int seclen = ((x * 3) + y) % 8;
    sec2       = _secGoogleWord.left(seclen);
    if (y >= 10000 && y < 100000) {
        sec1 = QStringLiteral("&s=");
    }
}

//-----------------------------------------------------------------------------
void GoogleCNMapProvider::_networkReplyError(QNetworkReply::NetworkError error) {
    qWarning() << "Could not connect to google maps. Error:" << error;
    if (_googleReply) {
        _googleReply->deleteLater();
        _googleReply = nullptr;
    }
}
//-----------------------------------------------------------------------------
void GoogleCNMapProvider::_replyDestroyed() {
    _googleReply = nullptr;
}

void GoogleCNMapProvider::_googleVersionCompleted() {
    if (!_googleReply || (_googleReply->error() != QNetworkReply::NoError)) {
        qDebug() << "Error collecting Google maps version info";
        return;
    }
    const QString html = QString(_googleReply->readAll());

#if defined(DEBUG_GOOGLE_MAPS)
    QString filename = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    filename += QStringLiteral("/google.output");
    QFile file(filename);
    if (file.open(QIODevice::ReadWrite)) {
        QTextStream stream(&file);
        stream << html << endl;
    }
#endif

    QRegExp reg(QStringLiteral("\"*https?://mt\\D?\\d..*/vt\\?lyrs=m@(\\d*)"), Qt::CaseInsensitive);
    if (reg.indexIn(html) != -1) {
        _versionGoogleMap = QString(QStringLiteral("m@%1")).arg(reg.capturedTexts().value(1, QString()));
    }
    reg = QRegExp(QStringLiteral("\"*https?://khm\\D?\\d.googleapis.cn/kh\\?v=(\\d*)"), Qt::CaseInsensitive);
    if (reg.indexIn(html) != -1) {
        _versionGoogleSatellite = reg.capturedTexts().value(1);
    }
    reg = QRegExp(QStringLiteral("\"*https?://mt\\D?\\d..*/vt\\?lyrs=t@(\\d*),r@(\\d*)"), Qt::CaseInsensitive);
    if (reg.indexIn(html) != -1) {
        const QStringList gc  = reg.capturedTexts();
        _versionGoogleTerrain = QString(QStringLiteral("t@%1,r@%2")).arg(gc.value(1), gc.value(2));
    }
    _googleReply->deleteLater();
    _googleReply = nullptr;
}

void GoogleCNMapProvider::_tryCorrectGoogleVersions(QNetworkAccessManager* networkManager) {
    QMutexLocker locker(&_googleVersionMutex);
    if (_googleVersionRetrieved) {
        return;
    }
    _googleVersionRetrieved = true;
    if (networkManager) {
        QNetworkRequest qheader;
        QNetworkProxy   proxy = networkManager->proxy();
        QNetworkProxy   tProxy;
        tProxy.setType(QNetworkProxy::DefaultProxy);
        networkManager->setProxy(tProxy);
        QSslConfiguration conf = qheader.sslConfiguration();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        qheader.setSslConfiguration(conf);
        const QString url = QStringLiteral("http://maps.google.cn/maps/api/js?v=3.2&sensor=false");
        qheader.setUrl(QUrl(url));
        QByteArray ua;
        ua.append(getQGCMapEngine()->userAgent());
        qheader.setRawHeader("User-Agent", ua);
        _googleReply = networkManager->get(qheader);
        connect(_googleReply, &QNetworkReply::finished, this, &GoogleCNMapProvider::_googleVersionCompleted);
        connect(_googleReply, &QNetworkReply::destroyed, this, &GoogleCNMapProvider::_replyDestroyed);
        connect(_googleReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error), this, &GoogleCNMapProvider::_networkReplyError);
        networkManager->setProxy(proxy);
    }
}

QString GoogleCNStreetMapProvider::_getURL(const int x, const int y, const int zoom, QNetworkAccessManager* networkManager) {
    // http://mt1.google.com/vt/lyrs=m
    QString server  = QStringLiteral("mt");
    QString request = QStringLiteral("vt");
    QString sec1; // after &x=...
    QString sec2; // after &zoom=...
    _getSecGoogleWords(x, y, sec1, sec2);
    _tryCorrectGoogleVersions(networkManager);
    return QString(QStringLiteral("http://%1%2.google.cn/%3/lyrs=%4&hl=%5&x=%6%7&y=%8&z=%9&s=%10"))
        .arg(server)
        .arg(_getServerNum(x, y, 4))
        .arg(request)
        .arg(_versionGoogleMap)
        .arg(_language)
        .arg(x)
        .arg(sec1)
        .arg(y)
        .arg(zoom)
        .arg(sec2);
}

QString GoogleCNSatelliteMapProvider::_getURL(const int x, const int y, const int zoom, QNetworkAccessManager* networkManager) {
    // http://mt1.google.com/vt/lyrs=s
    QString server  = QStringLiteral("mt");
    QString request = QStringLiteral("vt");
    QString sec1; // after &x=...
    QString sec2; // after &zoom=...
    _getSecGoogleWords(x, y, sec1, sec2);
    _tryCorrectGoogleVersions(networkManager);
    return QString(QStringLiteral("http://%1%2.google.cn/%3/lyrs=%4&gl=%5&x=%6%7&y=%8&z=%9&s=%10"))
        .arg(server)
        .arg(_getServerNum(x, y, 4))
        .arg(request)
        .arg(_versionGoogleSatellite)
        .arg(_language)
        .arg(x)
        .arg(sec1)
        .arg(y)
        .arg(zoom)
        .arg(sec2);
}

QString GoogleCNLabelsMapProvider::_getURL(const int x, const int y, const int zoom, QNetworkAccessManager* networkManager) {
    QString server  = "mts";
    QString request = "vt";
    QString sec1; // after &x=...
    QString sec2; // after &zoom=...
    _getSecGoogleWords(x, y, sec1, sec2);
    _tryCorrectGoogleVersions(networkManager);
    return QString(QStringLiteral("http://%1%2.google.cn/%3/lyrs=%4&hl=%5&x=%6%7&y=%8&z=%9&s=%10"))
        .arg(server)
        .arg(_getServerNum(x, y, 4))
        .arg(request)
        .arg(_versionGoogleLabels)
        .arg(_language)
        .arg(x)
        .arg(sec1)
        .arg(y)
        .arg(zoom)
        .arg(sec2);
}

QString GoogleCNTerrainMapProvider::_getURL(const int x, const int y, const int zoom, QNetworkAccessManager* networkManager) {
    QString server  = QStringLiteral("mt");
    QString request = QStringLiteral("vt");
    QString sec1; // after &x=...
    QString sec2; // after &zoom=...
    _getSecGoogleWords(x, y, sec1, sec2);
    _tryCorrectGoogleVersions(networkManager);
    return QString(QStringLiteral("http://%1%2.google.cn/%3/v=%4&hl=%5&x=%6%7&y=%8&z=%9&s=%10"))
        .arg(server)
        .arg(_getServerNum(x, y, 4))
        .arg(request)
        .arg(_versionGoogleTerrain)
        .arg(_language)
        .arg(x)
        .arg(sec1)
        .arg(y)
        .arg(zoom)
        .arg(sec2);
}

QString GoogleCNHybridMapProvider::_getURL(const int x, const int y, const int zoom, QNetworkAccessManager* networkManager) {
    QString server = QStringLiteral("mt");
    QString request = QStringLiteral("vt");
    QString sec1; // after &x=...
    QString sec2; // after &zoom=...
    _getSecGoogleWords(x, y, sec1, sec2);
    _tryCorrectGoogleVersions(networkManager);
    return QString(QStringLiteral("http://%1%2.google.cn/%3/lyrs=%4&hl=%5&x=%6%7&y=%8&z=%9&s=%10"))
        .arg(server)
        .arg(_getServerNum(x, y, 4))
        .arg(request)
        .arg(_versionGoogleHybrid)
        .arg(_language)
        .arg(x)
        .arg(sec1)
        .arg(y)
        .arg(zoom)
        .arg(sec2);
}
