#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <chrono>
#include <optional>
#include <random>
#include <functional>
#include <queue>
#include <concepts>
#include <stdexcept>

inline const char *TWITCH_API_VERSION_5="application/vnd.twitchtv.v5+json";
inline const short KEY=0;
inline const short VALUE=1;

namespace NumberConvert
{
	template <std::integral T>
	inline unsigned int Positive(T value)
	{
		if (static_cast<std::size_t>(std::abs(value)) > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) throw std::range_error("Overflow converting to positive integer");
		return value >= 0 ? static_cast<unsigned int>(value) : 0;
	}
}

namespace StringConvert
{
	inline QByteArray ByteArray(const QString &value) { return value.toLocal8Bit(); } // TODO: how do I report that this failed?
	inline QString Integer(const int &value)
	{
		QString result=QString::number(value);
		if (result.isEmpty()) throw std::range_error("Unable to convert number to text");
		return result;
	}
	inline int Integer(const QString &value)
	{
		bool succeeded=false;
		int result=value.toInt(&succeeded);
		if (!succeeded) throw std::range_error("Unable to convert text to number");
		return result;
	}
	inline QString PositiveInteger(const unsigned int &value)
	{
		QString result=QString::number(value);
		if (result.isEmpty()) throw std::range_error("Unable to convert number to text");
		return result;
	}
	inline unsigned int PositiveInteger(const QString &value)
	{
		bool succeeded=false;
		unsigned int result=value.toUInt(&succeeded);
		if (!succeeded) throw std::range_error("Unable to convert text to positive number");
		return result;
	}

	template <std::unsigned_integral T>
	inline QString NumberAgreement(const QString &singular,const QString &plural,T count)
	{
		return count == 1 ? singular : plural;
	}

	namespace Split
	{
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
		enum class Behaviors
		{
			KEEP_EMPTY_PARTS=QString::SplitBehavior::KeepEmptyParts,
			SKIP_EMPTY_PARTS=QString::SplitBehavior::SkipEmptyParts
		};
		inline QString::SplitBehavior Behavior(Behaviors behaviors) { return static_cast<QString::SplitBehavior>(behaviors); }
#else
		enum class Behaviors
		{
			KEEP_EMPTY_PARTS=Qt::KeepEmptyParts,
			SKIP_EMPTY_PARTS=Qt::SkipEmptyParts
		};
		inline Qt::SplitBehaviorFlags Behavior(Behaviors behaviors) { return static_cast<Qt::SplitBehaviorFlags>(behaviors); }
#endif
	}

	inline const QString Dump(const QString &data)
	{
#ifdef DEVELOPER_MODE
		QStringList lines=data.split("\n",Split::Behavior(Split::Behaviors::SKIP_EMPTY_PARTS));
		for (QString &line : lines) line.prepend("> ");
		return lines.join("\n");
#else
		return QString("> (data)");
#endif
	}
}

namespace TimeConvert
{
	constexpr std::chrono::seconds Seconds(const std::chrono::milliseconds &value) { return std::chrono::duration_cast<std::chrono::seconds>(value); }
	constexpr std::chrono::milliseconds Milliseconds(const std::chrono::seconds &value) { return std::chrono::duration_cast<std::chrono::milliseconds>(value); }
	constexpr int Interval(const std::chrono::seconds &value) { return value.count(); }
	constexpr int Interval(const std::chrono::milliseconds &value) { return value.count(); }
	constexpr std::chrono::seconds OneSecond() { return static_cast<std::chrono::seconds>(1); }
	inline const std::chrono::milliseconds Now() { return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch(); }
}

namespace Filesystem
{
	inline const QDir DataPath()
	{
		return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	}

	inline const QDir TemporaryPath()
	{
		return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	}

	const std::optional<QString> CreateHiddenFile(const QString &filePath);

	inline bool Touch(QFile &file)
	{
		const QDir path(QFileInfo(file).absolutePath());
		if (!file.exists())
		{
			if (!path.mkpath(path.absolutePath())) return false;
			if (!file.open(QIODevice::WriteOnly)) return false;
			file.close();
		}
		return true;
	}
}

namespace Random
{
	inline std::random_device generator;
	inline int Bounded(int lower,int upper)
	{
		std::uniform_int_distribution<int> distribution(lower,upper);
		return distribution(generator);
	}
	template<typename T> requires requires(T m) {
		requires std::forward_iterator<typename T::iterator>;
		{ m.empty() }->std::same_as<bool>;
		{ m.size() }->std::same_as<typename T::size_type>;
	}
	inline int Bounded(const T &container)
	{
		if (container.empty()) throw std::range_error("Tried to pull random item from empty container");
		if (container.size() > std::numeric_limits<int>::max()) throw std::range_error("Container contains too many elements");
		return Bounded(0,static_cast<int>(container.size())-1);
	}
}

namespace Network
{
	inline const char *CONTENT_TYPE="Content-Type";
	inline const char *CONTENT_TYPE_PLAIN="text/plain";
	inline const char *CONTENT_TYPE_JSON="application/json";
	inline const char *CONTENT_TYPE_FORM="application/x-www-form-urlencoded";

	static QNetworkAccessManager networkManager;

	enum class Method
	{
		GET,
		POST,
		PATCH
	};

	using Reply=std::function<void(QNetworkReply*)>;
	static std::queue<std::pair<std::function<void()>,Reply>> queue;

	inline void Request(QUrl url,Method method,Reply callback,const QUrlQuery &queryParameters=QUrlQuery(),const std::vector<std::pair<QByteArray,QByteArray>> &headers=std::vector<std::pair<QByteArray,QByteArray>>(),const QByteArray &payload=QByteArray())
	{
		networkManager.connect(&networkManager,&QNetworkAccessManager::finished,[](QNetworkReply *reply) {
			reply->deleteLater();
		});

		QNetworkRequest request;
		for (const std::pair<QByteArray,QByteArray> &header : headers) request.setRawHeader(header.first,header.second);
		switch (method)
		{
		case Method::GET:
		{
			url.setQuery(queryParameters);
			request.setUrl(url);
			auto sendRequest=[request,callback]() {
				QNetworkReply *reply=networkManager.get(request);
				networkManager.connect(&networkManager,&QNetworkAccessManager::finished,reply,callback);
				reply->connect(reply,&QNetworkReply::finished,[]() {
					queue.pop();
					if (queue.size() > 0) queue.front().first();
				});
			};
			if (queue.size() == 0) sendRequest();
			queue.push({sendRequest,callback});
			break;
		}
		case Method::POST:
		{
			request.setUrl(url);
			networkManager.connect(&networkManager,&QNetworkAccessManager::finished,networkManager.post(request,payload.isEmpty() ? StringConvert::ByteArray(queryParameters.query()) : payload),callback);
			break;
		}
		case Method::PATCH:
		{
			url.setQuery(queryParameters);
			request.setUrl(url);
			networkManager.connect(&networkManager,&QNetworkAccessManager::finished,networkManager.sendCustomRequest(request,"PATCH",payload),callback);
			break;
		}
		}
	}
}

namespace Platform
{
	constexpr bool Windows()
	{
#ifdef Q_OS_WIN
		return true;
#else
		return false;
#endif
	}
}
