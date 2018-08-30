/*
 * Copyright (C) 2013 ~ 2018 National University of Defense Technology(NUDT) & Tianjin Kylin Ltd.
 *
 * Authors:
 *  Kobe Lee    lixiang@kylinos.cn/kobe24_lixiang@126.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "weatherworker.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QEventLoop>

WeatherWorker::WeatherWorker(QObject *parent) :
    QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [] (QNetworkReply *reply) {
        reply->deleteLater();
    });
}

WeatherWorker::~WeatherWorker()
{

}

bool WeatherWorker::isNetWorkSettingsGood()
{
    //判断网络是否有连接，不一定能上网
    QNetworkConfigurationManager mgr;
    return mgr.isOnline();
}

void WeatherWorker::netWorkOnlineOrNot()
{
    //http://service.ubuntukylin.com:8001/weather/pingnetwork/
    QHostInfo::lookupHost("www.baidu.com", this, SLOT(networkLookedUp(QHostInfo)));
}

void WeatherWorker::networkLookedUp(const QHostInfo &host)
{
    if(host.error() != QHostInfo::NoError) {
        qDebug() << "test network failed, errorCode:" << host.error();
        emit this->nofityNetworkStatus(false);
    }
    else {
        qDebug() << "test network success, the server's ip:" << host.addresses().first().toString();
        emit this->nofityNetworkStatus(true);
    }
}

void WeatherWorker::refreshObserveWeatherData(const QString &cityId)
{
    if (cityId.isEmpty())
        return;

    /*QString forecastUrl = QString("http://service.ubuntukylin.com:8001/weather/api/1.0/observe/%1").arg(cityId);
    qDebug() << "forecastUrl=" << forecastUrl;
    QNetworkAccessManager *manager = new QNetworkAccessManager();
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(forecastUrl)));
    QByteArray responseData;
    QEventLoop eventLoop;
    QObject::connect(manager, SIGNAL(finished(QNetworkReply *)), &eventLoop, SLOT(quit()));
    eventLoop.exec();
    responseData = reply->readAll();
    reply->deleteLater();
    manager->deleteLater();
    qDebug() << "weather observe size: " << responseData.size();*/

    //heweather_observe_s6
    QString forecastUrl = QString("http://service.ubuntukylin.com:8001/weather/api/2.0/heweather_observe_s6/%1").arg(cityId);
    QNetworkRequest request;
    request.setUrl(forecastUrl);
    //request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);//Qt5.6 for redirect
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onWeatherObserveReply);
}

void WeatherWorker::refreshForecastWeatherData(const QString &cityId)
{
    if (cityId.isEmpty())
        return;

    //heweather_forecast_s6
    QString forecastUrl = QString("http://service.ubuntukylin.com:8001/weather/api/2.0/heweather_observe_s6/%1").arg(cityId);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(forecastUrl));
    connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onWeatherForecastReply);
}

void WeatherWorker::requestPingBackWeatherServer()
{
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(QString("http://service.ubuntukylin.com:8001/weather/pingnetwork/")));
    connect(reply, &QNetworkReply::finished, this, [=] () {
        QNetworkReply *m_reply = qobject_cast<QNetworkReply*>(sender());
        int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(m_reply->error() != QNetworkReply::NoError || statusCode != 200) {//200 is normal status
            qDebug() << "pingback request error:" << m_reply->error() << ", statusCode=" << statusCode;
            return;
        }

        QByteArray ba = m_reply->readAll();
        m_reply->close();
        m_reply->deleteLater();
        QString reply_content = QString::fromUtf8(ba);
        qDebug() << "pingback size: " << ba.size() << reply_content;
    });
}

void WeatherWorker::requestPostHostInfoToWeatherServer(QString hostInfo)
{
    this->m_hostInfoParameters = hostInfo;
    QByteArray parameters = hostInfo.toUtf8();
    QNetworkRequest request;
    request.setUrl(QUrl("http://service.ubuntukylin.com:8001/weather/pingbackmain"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::ContentLengthHeader, parameters.length());
    //QUrl url("http://service.ubuntukylin.com:8001/weather/pingbackmain");
    QNetworkReply *reply = m_networkManager->post(request, parameters);//QNetworkReply *reply = m_networkManager->post(QNetworkRequest(url), parameters);
    connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onPingBackPostReply);
}

void WeatherWorker::AccessDedirectUrl(const QString &redirectUrl, WeatherType weatherType)
{
    if (redirectUrl.isEmpty())
        return;

    QNetworkRequest request;
    QString url;
    url = redirectUrl;
    request.setUrl(QUrl(url));

    QNetworkReply *reply = m_networkManager->get(request);

    switch (weatherType) {
    case WeatherType::Type_Observe:
        connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onWeatherObserveReply);
        break;
    case WeatherType::Type_Forecast:
        connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onWeatherForecastReply);
        break;
    default:
        break;
    }
}

void WeatherWorker::AccessDedirectUrlWithPost(const QString &redirectUrl)
{
    if (redirectUrl.isEmpty())
        return;

    QNetworkRequest request;
    QString url;
    url = redirectUrl;
    QByteArray parameters = this->m_hostInfoParameters.toUtf8();
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::ContentLengthHeader, parameters.length());
    QNetworkReply *reply = m_networkManager->post(request, parameters);
    connect(reply, &QNetworkReply::finished, this, &WeatherWorker::onPingBackPostReply);
}

void WeatherWorker::onWeatherObserveReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(reply->error() != QNetworkReply::NoError || statusCode != 200) {//200 is normal status
        qDebug() << "weather request error:" << reply->error() << ", statusCode=" << statusCode;
        if (statusCode == 301 || statusCode == 302) {//redirect
            QVariant redirectionUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            qDebug() << "redirectionUrl=" << redirectionUrl.toString();
            AccessDedirectUrl(redirectionUrl.toString(), WeatherType::Type_Observe);//AccessDedirectUrl(reply->rawHeader("Location"));
            reply->close();
            reply->deleteLater();
        }
        return;
    }

    QByteArray ba = reply->readAll();
//    QString reply_content = QString::fromUtf8(ba);
    reply->close();
    reply->deleteLater();
    qDebug() << "weather observe size: " << ba.size();
//    QStringList dataList = reply_content.split("\n");
//    qDebug() << "---------------start---------------";
//    for (QString data : dataList) {
//        qDebug () << data;
//    }
//    qDebug() << "---------------end---------------";
    QJsonDocument jsonDocument = QJsonDocument::fromJson(ba);
    if(jsonDocument.isNull() ){
        qDebug()<< "===> QJsonDocument："<< ba;
    }
    QJsonObject jsonObject = jsonDocument.object();
    qDebug() << "jsonObject" << jsonObject;

    /*QJsonArray items = QJsonDocument::fromJson(ba).array();
    qDebug() << items;
    for (QJsonValue val : items) {
        QJsonObject json_obj = val.toObject();
        qDebug() << "---------------start---------------";
//        qDebug() << json_obj["city"].toString();
//        qDebug() << json_obj["WD"].toString();
//        qDebug() << json_obj["temp"].toString();
//        qDebug() << json_obj["ptime"].toString();
//        qDebug() << json_obj["temp2"].toString();
//        qDebug() << json_obj["temp1"].toString();
//        qDebug() << json_obj["weather"].toString();
//        qDebug() << json_obj["WS"].toString();
//        qDebug() << json_obj["time"].toString();
//        qDebug() << json_obj["img2"].toString();
//        qDebug() << json_obj["img1"].toString();
//        qDebug() << json_obj["aqi"].toString();
//        qDebug() << json_obj["SD"].toString();
        qDebug() << "---------------end---------------";
    }*/
    emit this->observeDataRefreshed();
}

void WeatherWorker::onWeatherForecastReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(reply->error() != QNetworkReply::NoError || statusCode != 200) {//200 is normal status
        qDebug() << "weather forecast request error:" << reply->error() << ", statusCode=" << statusCode;
        if (statusCode == 301 || statusCode == 302) {//redirect
            QVariant redirectionUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            qDebug() << "redirectionUrl=" << redirectionUrl.toString();
            AccessDedirectUrl(redirectionUrl.toString(), WeatherType::Type_Forecast);//AccessDedirectUrl(reply->rawHeader("Location"));
            reply->close();
            reply->deleteLater();
        }
        return;
    }

    QByteArray ba = reply->readAll();
    reply->close();
    reply->deleteLater();
//    qDebug() << "weather forecast size: " << ba.size();
    QJsonArray items = QJsonDocument::fromJson(ba).array();
    for (QJsonValue val : items) {
//        QJsonObject obj = val.toObject();
    }

    emit this->forecastDataRefreshed();
}

void WeatherWorker::onPingBackPostReply()
{
    QNetworkReply *m_reply = qobject_cast<QNetworkReply*>(sender());
    int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(m_reply->error() != QNetworkReply::NoError || statusCode != 200) {//200 is normal status
        qDebug() << "post host info request error:" << m_reply->error() << ", statusCode=" << statusCode;
        if (statusCode == 301 || statusCode == 302) {//redirect
            QVariant redirectionUrl = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
            qDebug() << "pingback redirectionUrl=" << redirectionUrl.toString();
            AccessDedirectUrlWithPost(redirectionUrl.toString());
            m_reply->close();
            m_reply->deleteLater();
        }
        return;
    }

    QByteArray ba = m_reply->readAll();
    m_reply->close();
    m_reply->deleteLater();
    QString reply_content = QString::fromUtf8(ba);
    qDebug() << "return size: " << ba.size() << reply_content;
}

/*  http://www.heweather.com/documents/status-code  */
QString WeatherWorker::getErrorCodeDescription(QString errorCode)
{
    if ("ok" == errorCode) {
        return "数据正常";
    }
    else if ("invalid key" == errorCode) {
        return "错误的key，请检查你的key是否输入以及是否输入有误";
    }
    else if ("unknown location" == errorCode) {
        return "未知或错误城市/地区";
    }
    else if ("no data for this location" == errorCode) {
        return "该城市/地区没有你所请求的数据";
    }
    else if ("no more requests" == errorCode) {
        return "超过访问次数，需要等到当月最后一天24点（免费用户为当天24点）后进行访问次数的重置或升级你的访问量";
    }
    else if ("param invalid" == errorCode) {
        return "参数错误，请检查你传递的参数是否正确";
    }
    else if ("too fast" == errorCode) {//http://www.heweather.com/documents/qpm
        return "超过限定的QPM，请参考QPM说明";
    }
    else if ("dead" == errorCode) {//http://www.heweather.com/contact
        return "无响应或超时，接口服务异常请联系我们";
    }
    else if ("permission denied" == errorCode) {
        return "无访问权限，你没有购买你所访问的这部分服务";
    }
    else if ("sign error" == errorCode) {//http://www.heweather.com/documents/api/s6/sercet-authorization
        return "签名错误，请参考签名算法";
    }
    else {
        return tr("Unknown");
    }
}
