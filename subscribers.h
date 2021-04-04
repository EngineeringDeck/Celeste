#pragma once

#include <QTcpServer>
#include "settings.h"

inline const char *SUBSCRIPTION_TYPE_FOLLOW="channel.follow";
inline const char *SUBSCRIPTION_TYPE_REDEPTION="channel.challen_points_custom_reward_redemption.add";

enum class SubscriptionType
{
	CHANNEL_FOLLOW
};

class EventSubscriber : public QTcpServer
{
	Q_OBJECT
	using Headers=std::unordered_map<QString,QString>;
	using SubscriptionTypes=std::unordered_map<QString,SubscriptionType>;
public:
	EventSubscriber(QObject *parent=nullptr);
	~EventSubscriber();
	void Listen();
	void Subscribe(const QString &type);
	void DataAvailable();
protected:
	static const QString SUBSYSTEM_NAME;
	static const QString LINE_BREAK;
	Setting settingClientID;
	Setting settingOAuthToken;
	const QString secret;
	QString buffer;
	SubscriptionTypes subscriptionTypes; // TODO: find a better name for this
	const Headers ParseHeaders(const QString &data);
	const QByteArray ProcessRequest(const SubscriptionType type,const QString &data);
	const QString BuildResponse(const QString &data=QString()) const;
	virtual QTcpSocket* SocketFromSignalSender() const;
	virtual QByteArray ReadFromSocket(QTcpSocket *socket=nullptr) const;
	virtual bool WriteToSocket(const QByteArray &data,QTcpSocket *socket=nullptr) const;
signals:
	void Print(const QString &message) const;
protected slots:
	void ConnectionAvailable();
};
