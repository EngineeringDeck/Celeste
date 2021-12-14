#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QApplication>
#include <QTimeZone>
#include <QNetworkReply>
#include "bot.h"
#include "globals.h"

const char *COMMANDS_LIST_FILENAME="commands.json";
const char *JSON_KEY_COMMAND_NAME="command";
const char *JSON_KEY_COMMAND_ALIASES="aliases";
const char *JSON_KEY_COMMAND_DESCRIPTION="description";
const char *JSON_KEY_COMMAND_TYPE="type";
const char *JSON_KEY_COMMAND_RANDOM_PATH="random";
const char *JSON_KEY_COMMAND_PATH="path";
const char *JSON_KEY_COMMAND_MESSAGE="message";
const char *SETTINGS_CATEGORY_EVENTS="Events";
const char *SETTINGS_CATEGORY_VIBE="Vibe";
const char *TWITCH_API_ENDPOINT_EMOTE_URL="https://static-cdn.jtvnw.net/emoticons/v1/%1/1.0";

const std::unordered_map<QString,CommandType> COMMAND_TYPES={
	{"native",CommandType::NATIVE},
	{"video",CommandType::VIDEO},
	{"announce",CommandType::AUDIO}
};

std::chrono::milliseconds Bot::launchTimestamp=TimeConvert::Now();

Bot::Bot(PrivateSetting &settingAdministrator,PrivateSetting &settingOAuthToken,PrivateSetting &settingClientID,QObject *parent) : QObject(parent),
	downloadManager(new QNetworkAccessManager(this)),
	vibeKeeper(new QMediaPlayer(this)),
	vibeFader(nullptr),
	roaster(new QMediaPlayer(this)),
	settingAdministrator(settingAdministrator),
	settingOAuthToken(settingOAuthToken),
	settingClientID(settingClientID),
	settingInactivityCooldown(SETTINGS_CATEGORY_EVENTS,"InactivityCooldown",1800000),
	settingHelpCooldown(SETTINGS_CATEGORY_EVENTS,"HelpCooldown",300000),
	settingVibePlaylist(SETTINGS_CATEGORY_VIBE,"Playlist"),
	settingRoasts(SETTINGS_CATEGORY_EVENTS,"Roasts"),
	settingPortraitVideo(SETTINGS_CATEGORY_EVENTS,"Portrait"),
	settingArrivalSound(SETTINGS_CATEGORY_EVENTS,"Arrival"),
	settingCheerVideo(SETTINGS_CATEGORY_EVENTS,"Cheer"),
	settingSubscriptionSound(SETTINGS_CATEGORY_EVENTS,"Subscription"),
	settingRaidSound(SETTINGS_CATEGORY_EVENTS,"Raid"),
	settingRaidInterruptDuration(SETTINGS_CATEGORY_EVENTS,"RaidInterruptDelay",60000)
{
	Command agendaCommand("agenda","Set the agenda of the stream, displayed in the header of the chat window",CommandType::NATIVE,true);
	Command commandsCommand("commands","List all of the commands Celeste recognizes",CommandType::NATIVE,false);
	Command panicCommand("panic","Crash Celeste",CommandType::NATIVE,true);
	Command shoutOutCommand("so","Call attention to another streamer's channel",CommandType::NATIVE,false);
	Command songCommand("song","Show the title, album, and artist of the song that is currently playing",CommandType::NATIVE,false);
	Command timezoneCommand("timezone","Display the timezone of the system the bot is running on",CommandType::NATIVE,false);
	Command uptimeCommand("uptime","Show how long the bot has been connected",CommandType::NATIVE,false);
	Command vibeCommand("vibe","Start the playlist of music for the stream",CommandType::NATIVE,true);
	Command volumeCommand("volume","Adjust the volume of the vibe keeper",CommandType::NATIVE,true);
	commands.insert({
		{agendaCommand.Name(),agendaCommand},
		{commandsCommand.Name(),commandsCommand},
		{panicCommand.Name(),panicCommand},
		{shoutOutCommand.Name(),shoutOutCommand},
		{songCommand.Name(),songCommand},
		{timezoneCommand.Name(),timezoneCommand},
		{uptimeCommand.Name(),uptimeCommand},
		{vibeCommand.Name(),vibeCommand},
		{volumeCommand.Name(),volumeCommand}
	});
	nativeCommandFlags.insert({
		{agendaCommand.Name(),NativeCommandFlag::AGENDA},
		{commandsCommand.Name(),NativeCommandFlag::COMMANDS},
		{panicCommand.Name(),NativeCommandFlag::PANIC},
		{shoutOutCommand.Name(),NativeCommandFlag::SHOUTOUT},
		{songCommand.Name(),NativeCommandFlag::SONG},
		{timezoneCommand.Name(),NativeCommandFlag::TIMEZONE},
		{uptimeCommand.Name(),NativeCommandFlag::UPTIME},
		{vibeCommand.Name(),NativeCommandFlag::VIBE},
		{volumeCommand.Name(),NativeCommandFlag::VOLUME}
	});
	if (!LoadDynamicCommands()) emit Print("Failed to load commands");

	if (settingVibePlaylist) LoadVibePlaylist();
	if (settingRoasts) LoadRoasts();
	StartClocks();

	lastRaid=QDateTime::currentDateTime().addMSecs(static_cast<qint64>(0)-static_cast<qint64>(settingRaidInterruptDuration));
	vibeKeeper->setVolume(0);
}

bool Bot::LoadDynamicCommands()
{
	QFile commandListFile(Filesystem::DataPath().filePath(COMMANDS_LIST_FILENAME));
	if (!commandListFile.open(QIODevice::ReadWrite))
	{
		emit Print(QString("Failed to open command list file: %1").arg(commandListFile.fileName()));
		return false;
	}

	QJsonParseError jsonError;
	QByteArray data=commandListFile.readAll();
	if (data.isEmpty()) data="[]";
	QJsonDocument json=QJsonDocument::fromJson(data,&jsonError);
	if (json.isNull())
	{
		emit Print(jsonError.errorString());
		return false;
	}

	for (const QJsonValue &jsonValue : json.array())
	{
		QJsonObject jsonObject=jsonValue.toObject();
		const QString name=jsonObject.value(JSON_KEY_COMMAND_NAME).toString();
		commands[name]={
			name,
			jsonObject.value(JSON_KEY_COMMAND_DESCRIPTION).toString(),
			COMMAND_TYPES.at(jsonObject.value(JSON_KEY_COMMAND_TYPE).toString()),
			jsonObject.contains(JSON_KEY_COMMAND_RANDOM_PATH) ? jsonObject.value(JSON_KEY_COMMAND_RANDOM_PATH).toBool() : false,
			jsonObject.value(JSON_KEY_COMMAND_PATH).toString(),
			jsonObject.contains(JSON_KEY_COMMAND_MESSAGE) ? jsonObject.value(JSON_KEY_COMMAND_MESSAGE).toString() : QString()
		};
		if (jsonObject.contains(JSON_KEY_COMMAND_ALIASES))
		{
			for (const QJsonValue &jsonValue : jsonObject.value(JSON_KEY_COMMAND_ALIASES).toArray())
			{
				const QString alias=jsonValue.toString();
				commands[alias]={
					alias,
					commands.at(name).Description(),
					commands.at(name).Type(),
					commands.at(name).Random(),
					commands.at(name).Path(),
					commands.at(name).Message(),
				};
			}
		}
	}

	return true;
}

void Bot::LoadVibePlaylist()
{
	emit Print("viberator");
	connect(&vibeSources,&QMediaPlaylist::loadFailed,[this]() {
		emit Print(QString("Failed to load vibe playlist: %1").arg(vibeSources.errorString()));
	});
	connect(&vibeSources,&QMediaPlaylist::loaded,[this]() {
		vibeSources.shuffle();
		vibeSources.setCurrentIndex(Random::Bounded(0,vibeSources.mediaCount()));
		vibeKeeper->setPlaylist(&vibeSources);
		emit Print("Vibe playlist loaded!");
	});
	connect(vibeKeeper,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),[this](QMediaPlayer::Error error) {
		emit Print(QString("Vibe Keeper failed to start: %1").arg(vibeKeeper->errorString()));
	});
	connect(vibeKeeper,&QMediaPlayer::stateChanged,[this](QMediaPlayer::State state) {
		if (state == QMediaPlayer::PlayingState) emit Print(QString("Now playing %1 by %2").arg(vibeKeeper->metaData("Title").toString(),vibeKeeper->metaData("AlbumArtist").toString()));
	});
	vibeSources.load(QUrl::fromLocalFile(settingVibePlaylist));
}

void Bot::LoadRoasts()
{
	connect(&roastSources,&QMediaPlaylist::loadFailed,[this]() {
		emit Print(QString("Failed to load roasts: %1").arg(roastSources.errorString()));
	});
	connect(&roastSources,&QMediaPlaylist::loaded,[this]() {
		roastSources.shuffle();
		roastSources.setCurrentIndex(Random::Bounded(0,roastSources.mediaCount()));
		roaster->setPlaylist(&roastSources);
		connect(&inactivityClock,&QTimer::timeout,roaster,&QMediaPlayer::play);
		emit Print("Roasts loaded!");
	});
	roastSources.load(QUrl::fromLocalFile(settingRoasts));
	connect(roaster,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),[this](QMediaPlayer::Error error) {
		emit Print(QString("Roaster failed to start: %1").arg(vibeKeeper->errorString()));
	});
	connect(roaster,&QMediaPlayer::mediaStatusChanged,[this](QMediaPlayer::MediaStatus status) {
		if (status == QMediaPlayer::EndOfMedia)
		{
			roaster->stop();
			roastSources.setCurrentIndex(Random::Bounded(0,roastSources.mediaCount()));
		}
	});
}

void Bot::StartClocks()
{
	inactivityClock.setInterval(TimeConvert::Interval(std::chrono::milliseconds(settingInactivityCooldown)));
	if (settingPortraitVideo)
	{
		connect(&inactivityClock,&QTimer::timeout,[this]() {
			emit PlayVideo(settingPortraitVideo);
		});
	}
	inactivityClock.start();

	helpClock.setInterval(TimeConvert::Interval(std::chrono::milliseconds(settingHelpCooldown)));
	connect(&helpClock,&QTimer::timeout,[this]() {
		std::unordered_map<QString,Command>::iterator candidate=std::next(std::begin(commands),Random::Bounded(commands));
		emit ShowCommand(candidate->second.Name(),candidate->second.Description());
	});
	helpClock.start();
}

void Bot::Ping()
{
	if (settingPortraitVideo)
		ShowPortraitVideo(settingPortraitVideo);
	else
		Print("Letting Twitch server know we're still here...");
}

void Bot::Subscription(const QString &viewer)
{
	if (static_cast<QString>(settingSubscriptionSound).isEmpty())
	{
		emit Print("No audio path set for subscriptions","announce subscription");
		return;
	}
	emit AnnounceSubscription(viewer,settingSubscriptionSound);
}

void Bot::Raid(const QString &viewer,const unsigned int viewers)
{
	lastRaid=QDateTime::currentDateTime();
	emit AnnounceRaid(viewer,viewers,settingRaidSound);
}

void Bot::Cheer(const QString &viewer,const unsigned int count,const QString &message)
{
	if (static_cast<QString>(settingCheerVideo).isEmpty())
	{
		emit Print("No video path set for cheers","announce cheer");
		return;
	}
	emit AnnounceCheer(viewer,count,message,settingCheerVideo);
}

void Bot::DispatchArrival(Viewer::Local viewer)
{
	if (static_cast<QString>(settingArrivalSound).isEmpty())
	{
		emit Print("No audio path set for arrivals","announce arrival");
		return;
	}
	if (settingAdministrator != viewer.Name() && QDateTime::currentDateTime().toMSecsSinceEpoch()-lastRaid.toMSecsSinceEpoch() > static_cast<qint64>(settingRaidInterruptDuration))
	{
		Viewer::ProfileImage::Remote *profileImage=viewer.ProfileImage();
		connect(profileImage,&Viewer::ProfileImage::Remote::Retrieved,[this,viewer](const QImage &profileImage) {
			emit AnnounceArrival(viewer.DisplayName(),profileImage,File::List(settingArrivalSound).Random());
		});
		connect(profileImage,&Viewer::ProfileImage::Remote::Print,this,&Bot::Print);
	}
}

void Bot::ParseChatMessage(const QString &message)
{
	QStringList messageSegments;
	messageSegments=message.split(" ",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::KEEP_EMPTY_PARTS));
	if (messageSegments.size() < 2)
	{
		emit Print("Message is invalid");
		return;
	}

	// extract some metadata that appears before the message
	// tags
	TagMap tags=TakeTags(messageSegments);
	if (messageSegments=messageSegments.join(" ").split(":",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::SKIP_EMPTY_PARTS)); messageSegments.size() < 2)
	{
		emit Print("Invalid message segment count");
		return;
	}
	// username/viewer
	std::optional<QStringList> hostmask=TakeHostmask(messageSegments);
	if (!hostmask)
	{
		emit Print("Invalid message sender");
		return;
	}
	QString token=settingOAuthToken;
	QString client=settingClientID;
	Viewer::Remote *viewer=new Viewer::Remote(hostmask->first().trimmed(),settingOAuthToken,settingClientID);
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
	connect(viewer,&Viewer::Remote::Recognized,viewer,[this,messageSegments,tags](Viewer::Local viewer) mutable {
		inactivityClock.start();

		if (viewers.find(viewer.Name()) == viewers.end())
		{
			DispatchArrival(viewer);
			viewers.emplace(viewer.Name(),viewer);
		}

		// determine if this is a command, and if so, process it as such
		messageSegments=messageSegments.join(":").trimmed().split(" ",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::KEEP_EMPTY_PARTS));
		if (messageSegments.at(0).at(0) == '!')
		{
			QString commandName=messageSegments.takeFirst();
			if (DispatchCommand(commandName.mid(1),messageSegments.join(" "),viewer,Broadcaster(tags))) return;
			messageSegments.prepend(commandName);
		}

		// determine if the message is an action
		// have to do this here, because the ACTION tag
		// throws off the indicies in processing emotes below
		bool action=false;
		if (messageSegments.front() == "\001ACTION")
		{
			messageSegments.pop_front();
			messageSegments.back().remove('\001');
			action=true;
		}
		QString message=messageSegments.join(" ");

		// look for emotes if they exist
		std::vector<Media::Emote> emotes;
		if (tags.find("emotes") != tags.end())
		{
			emotes=ParseEmotes(tags,message);
		}

		// print the final message
		const QString &color=tags.at("color");
		emit ChatMessage(viewer.DisplayName(),message,emotes,color.isNull() ? QColor() : QColor(color),action);
	});
}

bool Bot::Broadcaster(TagMap &tags)
{
	if (tags.find("badges") == tags.end()) return false;
	QString badges=tags.at("badges");
	// FIXME: finish this
	return true;
}

Bot::TagMap Bot::TakeTags(QStringList &messageSegments)
{
	TagMap result;
	for (const QString &pair : messageSegments.takeFirst().split(";",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::SKIP_EMPTY_PARTS)))
	{
		QStringList components=pair.split("=",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::KEEP_EMPTY_PARTS));
		if (components.size() == 2)
			result[components.at(KEY)]=components.at(VALUE);
		else
			emit Print(QString("Message has an invalid tag: %1").arg(pair));
	}
	if (result.find("color") == result.end())
	{
		emit Print("Message has no color");
		result["color"]=QString();
	}
	return result;
}

std::optional<QStringList> Bot::TakeHostmask(QStringList &messageSegments)
{
	QStringList hostmaskSegments=messageSegments.takeFirst().split("!",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::SKIP_EMPTY_PARTS));
	if (hostmaskSegments.size() < 1)
	{
		emit Print("Message hostmask is invalid");
		return std::nullopt;
	}
	return hostmaskSegments;
}

void Bot::DownloadEmote(const Media::Emote &emote)
{
	if (!QFile(emote.Path()).exists())
	{
		QNetworkRequest request(QString(TWITCH_API_ENDPOINT_EMOTE_URL).arg(emote.ID()));
		QNetworkReply *downloadReply=downloadManager->get(request);
		connect(downloadReply,&QNetworkReply::finished,[this,downloadReply,emote]() {
			downloadReply->deleteLater();
			if (downloadReply->error())
			{
				emit Print(QString("Failed to download emote %1: %2").arg(emote.Name(),downloadReply->errorString()));
				return;
			}
			if (!QImage::fromData(downloadReply->readAll()).save(emote.Path())) emit Print(QString("Failed to save emote %1 to %2").arg(emote.Name(),emote.Path()));
			emit RefreshChat();
		});
	}
}

const std::vector<Media::Emote> Bot::ParseEmotes(const TagMap &tags,const QString &message)
{
	std::vector<Media::Emote> emotes;
	QStringList entries=tags.at("emotes").split("/",StringConvert::Split::Behavior(StringConvert::Split::Behaviors::SKIP_EMPTY_PARTS));
	for (const QString &entry : entries)
	{
		QStringList details=entry.split(":");
		if (details.size() < 2)
		{
			emit Print("Malformatted emote in message");
			continue;
		}
		for (const QString &instance : details.at(1).split(","))
		{
			QStringList indicies=instance.split("-");
			if (indicies.size() < 2)
			{
				emit Print("Cannot determine where emote starts or ends");
				continue;
			}
			unsigned int start=StringConvert::PositiveInteger(indicies.at(0));
			unsigned int end=StringConvert::PositiveInteger(indicies.at(1));
			const Media::Emote emote({message.mid(start,end-start),details.at(0),Filesystem::TemporaryPath().filePath(QString("%1.png").arg(details.at(0))),start,end});
			DownloadEmote(emote);
			emotes.push_back(emote);
		}
	}
	std::sort(emotes.begin(),emotes.end());
	return emotes;
}

bool Bot::DispatchCommand(const QString name,const QString parameters,const Viewer::Local viewer,bool broadcaster)
{
	if (commands.find(name) == commands.end()) return false;
	Command command=parameters.isEmpty() ? commands.at(name) : Command(commands.at(name),parameters);

	if (command.Protected() && !broadcaster)
	{
		emit Print(QString("The command %1 is protected but %2 is not the broadcaster").arg(command.Name(),viewer.Name()));
		return false;
	}

	switch (command.Type())
	{
	case CommandType::VIDEO:
		DispatchVideo(command);
		break;
	case CommandType::AUDIO:
		emit PlayAudio(viewer.DisplayName(),command.Message(),command.Path());
		break;
	case CommandType::NATIVE:
		switch (nativeCommandFlags.at(command.Name()))
		{
		case NativeCommandFlag::AGENDA:
			emit SetAgenda(command.Message());
			break;
		case NativeCommandFlag::COMMANDS:
			DispatchCommandList();
			break;
		case NativeCommandFlag::PANIC:
			DispatchPanic();
			break;
		case NativeCommandFlag::SHOUTOUT:
			DispatchShoutout(command);
			break;
		case NativeCommandFlag::SONG:
			emit ShowCurrentSong(vibeKeeper->metaData("Title").toString(),vibeKeeper->metaData("AlbumTitle").toString(),vibeKeeper->metaData("AlbumArtist").toString(),vibeKeeper->metaData("CoverArtImage").value<QImage>());
			break;
		case NativeCommandFlag::TIMEZONE:
			emit ShowTimezone(QDateTime::currentDateTime().timeZone().displayName(QDateTime::currentDateTime().timeZone().isDaylightTime(QDateTime::currentDateTime()) ? QTimeZone::DaylightTime : QTimeZone::StandardTime,QTimeZone::LongName));
			break;
		case NativeCommandFlag::UPTIME:
			DispatchUptime();
			break;
		case NativeCommandFlag::VIBE:
			ToggleVibeKeeper();
			break;
		case NativeCommandFlag::VOLUME:
			AdjustVibeVolume(command);
			break;
		}
		break;
	};

	return true;
}

void Bot::DispatchVideo(Command command)
{
	if (command.Random())
		DispatchRandomVideo(command);
	else
		emit PlayVideo(command.Path());
}

void Bot::DispatchRandomVideo(Command command)
{
	QDir directory(command.Path());
	QStringList videos=directory.entryList(QStringList() << "*.mp4",QDir::Files);
	if (videos.size() < 1)
	{
		emit Print("No videos found");
		return;
	}
	emit PlayVideo(directory.absoluteFilePath(videos.at(Random::Bounded(0,videos.size()))));
}

void Bot::DispatchCommandList()
{
	std::vector<std::pair<QString,QString>> descriptions;
	for (const std::pair<QString,Command> &command : commands)
	{
		if (!command.second.Protected()) descriptions.push_back({command.first,command.second.Description()});
	}
	emit ShowCommandList(descriptions);
}

void Bot::DispatchPanic()
{
	QFile outputFile(Filesystem::DataPath().absoluteFilePath("panic.txt"));
	QString outputText;
	if (outputFile.open(QIODevice::ReadOnly))
	{
		outputText=outputFile.readAll();
		outputFile.close();
	}
	QString date=QDateTime::currentDateTime().toString("ddd d hh:mm:ss");
	outputText=outputText.split("\n").join(QString("\n%1 ").arg(date));
	emit Panic(date+"\n"+outputText);
	// FIXME: figure out how to shut the bot down without closing the window
}

void Bot::DispatchShoutout(Command command)
{
	Viewer::Remote *viewer=new Viewer::Remote(QString(command.Message()).remove("@"),settingOAuthToken,settingClientID);
	connect(viewer,&Viewer::Remote::Recognized,[this](Viewer::Local viewer) {
		Viewer::ProfileImage::Remote *profileImage=viewer.ProfileImage();
		connect(profileImage,&Viewer::ProfileImage::Remote::Retrieved,[this,viewer](const QImage &profileImage) {
			emit Shoutout(viewer.DisplayName(),viewer.Description(),profileImage);
		});
	});
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
}

void Bot::DispatchUptime()
{
	std::chrono::milliseconds duration=TimeConvert::Now()-launchTimestamp;
	std::chrono::hours hours=std::chrono::duration_cast<std::chrono::hours>(duration);
	std::chrono::minutes minutes=std::chrono::duration_cast<std::chrono::minutes>(duration-hours);
	std::chrono::seconds seconds=std::chrono::duration_cast<std::chrono::seconds>(duration-minutes);
	emit ShowUptime(hours,minutes,seconds);
}

void Bot::ToggleVibeKeeper()
{
	if (vibeKeeper->state() == QMediaPlayer::PlayingState)
	{
		emit Print("Pausing the vibes...");
		vibeKeeper->pause();
	}
	vibeKeeper->play();
}

void Bot::AdjustVibeVolume(Command command)
{
	if (command.Message().isEmpty())
	{
		if (vibeFader)
		{
			vibeFader->Abort();
			emit Print("Aborting volume change...");
		}
		return;
	}

	try
	{
		vibeFader=new Volume::Fader(vibeKeeper,command.Message(),this);
		connect(vibeFader,&Volume::Fader::Print,this,&Bot::Print);
		vibeFader->Start();
	}
	catch (const std::range_error &exception)
	{
		emit Print(QString("%1: %2").arg("Failed to adjust volume",exception.what()));
	}
}