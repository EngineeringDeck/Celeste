#include <QFile>

#include <QApplication>
#include <QTimeZone>
#include <QNetworkReply>
#include "bot.h"
#include "globals.h"

const char *COMMANDS_LIST_FILENAME="commands.json";
const char *COMMAND_TYPE_NATIVE="native";
const char *COMMAND_TYPE_AUDIO="announce";
const char *COMMAND_TYPE_VIDEO="video";
const char *COMMAND_TYPE_PULSAR="pulsar";
const char *VIEWER_ATTRIBUTES_FILENAME="viewers.json";
const char *NETWORK_HEADER_AUTHORIZATION="Authorization";
const char *NETWORK_HEADER_CLIENT_ID="Client-Id";
const char *QUERY_PARAMETER_BROADCASTER_ID="broadcaster_id";
const char *QUERY_PARAMETER_MODERATOR_ID="moderator_id";
const char *JSON_KEY_COMMAND_NAME="command";
const char *JSON_KEY_COMMAND_ALIASES="aliases";
const char *JSON_KEY_COMMAND_DESCRIPTION="description";
const char *JSON_KEY_COMMAND_TYPE="type";
const char *JSON_KEY_COMMAND_RANDOM_PATH="random";
const char *JSON_KEY_COMMAND_PROTECTED="protected";
const char *JSON_KEY_COMMAND_PATH="path";
const char *JSON_KEY_COMMAND_MESSAGE="message";
const char *JSON_KEY_DATA="data";
const char *JSON_KEY_COMMANDS="commands";
const char *JSON_KEY_WELCOME="welcomed";
const char *JSON_KEY_BOT="bot";
const char *SETTINGS_CATEGORY_EVENTS="Events";
const char *SETTINGS_CATEGORY_VIBE="Vibe";
const char *SETTINGS_CATEGORY_COMMANDS="Commands";
const char *TWITCH_API_ENDPOINT_EMOTE_URL="https://static-cdn.jtvnw.net/emoticons/v1/%1/1.0";
const char *TWITCH_API_ENDPOINT_CHAT_SETTINGS="https://api.twitch.tv/helix/chat/settings";
const char *TWITCH_API_ENDPOINT_STREAM_INFORMATION="https://api.twitch.tv/helix/streams";
const char *TWITCH_API_ENDPOINT_CHANNEL_INFORMATION="https://api.twitch.tv/helix/channels";
const char *TWITCH_API_ENDPOINT_GAME_INFORMATION="https://api.twitch.tv/helix/games";
const char *TWITCH_API_ENDPOING_USER_FOLLOWS="https://api.twitch.tv/helix/users/follows";
const char *TWITCH_API_ENDPOINT_BADGES="https://api.twitch.tv/helix/chat/badges/global";
const char16_t *TWITCH_SYSTEM_ACCOUNT_NAME=u"jtv";
const char16_t *CHAT_BADGE_BROADCASTER=u"broadcaster";
const char16_t *CHAT_BADGE_MODERATOR=u"moderator";
const char16_t *CHAT_TAG_DISPLAY_NAME=u"display-name";
const char16_t *CHAT_TAG_BADGES=u"badges";
const char16_t *CHAT_TAG_COLOR=u"color";
const char16_t *CHAT_TAG_EMOTES=u"emotes";
const char *FILE_ERROR_TEMPLATE_COMMANDS_LIST_CREATE="Failed to create command list file: %1";
const char *FILE_ERROR_TEMPLATE_COMMANDS_LIST_OPEN="Failed to open command list file: %1";
const char *FILE_ERROR_TEMPLATE_COMMANDS_LIST_PARSE="Failed to parse command list file: %1";

const std::unordered_map<QString,CommandType> COMMAND_TYPES={
	{COMMAND_TYPE_NATIVE,CommandType::NATIVE},
	{COMMAND_TYPE_VIDEO,CommandType::VIDEO},
	{COMMAND_TYPE_AUDIO,CommandType::AUDIO},
	{COMMAND_TYPE_PULSAR,CommandType::PULSAR}
};

std::unordered_map<QString,std::unordered_map<QString,QString>> Bot::badgeIconURLs;
std::chrono::milliseconds Bot::launchTimestamp=TimeConvert::Now();

Bot::Bot(Security &security,QObject *parent) : QObject(parent),
	vibeKeeper(new Music::Player(this)),
	roaster(new QMediaPlayer(this)),
	security(security),
	settingInactivityCooldown(SETTINGS_CATEGORY_EVENTS,"InactivityCooldown",1800000),
	settingHelpCooldown(SETTINGS_CATEGORY_EVENTS,"HelpCooldown",300000),
	settingVibePlaylist(SETTINGS_CATEGORY_VIBE,"Playlist"),
	settingTextWallThreshold(SETTINGS_CATEGORY_EVENTS,"TextWallThreshold",400),
	settingTextWallSound(SETTINGS_CATEGORY_EVENTS,"TextWallSound"),
	settingRoasts(SETTINGS_CATEGORY_EVENTS,"Roasts"),
	settingPortraitVideo(SETTINGS_CATEGORY_EVENTS,"Portrait"),
	settingArrivalSound(SETTINGS_CATEGORY_EVENTS,"Arrival"),
	settingCheerVideo(SETTINGS_CATEGORY_EVENTS,"Cheer"),
	settingSubscriptionSound(SETTINGS_CATEGORY_EVENTS,"Subscription"),
	settingRaidSound(SETTINGS_CATEGORY_EVENTS,"Raid"),
	settingRaidInterruptDuration(SETTINGS_CATEGORY_EVENTS,"RaidInterruptDelay",60000),
	settingHostSound(SETTINGS_CATEGORY_EVENTS,"Host"),
	settingDeniedCommandVideo(SETTINGS_CATEGORY_COMMANDS,"Denied"),
	settingUptimeHistory(SETTINGS_CATEGORY_COMMANDS,"UptimeHistory",0),
	settingCommandNameAgenda(SETTINGS_CATEGORY_COMMANDS,"Agenda","agenda"),
	settingCommandNameStreamCategory(SETTINGS_CATEGORY_COMMANDS,"StreamCategory","category"),
	settingCommandNameStreamTitle(SETTINGS_CATEGORY_COMMANDS,"StreamTitle","title"),
	settingCommandNameCommands(SETTINGS_CATEGORY_COMMANDS,"Commands","commands"),
	settingCommandNameEmote(SETTINGS_CATEGORY_COMMANDS,"EmoteOnly","emote"),
	settingCommandNameFollowage(SETTINGS_CATEGORY_COMMANDS,"Followage","followage"),
	settingCommandNameHTML(SETTINGS_CATEGORY_COMMANDS,"HTML","html"),
	settingCommandNamePanic(SETTINGS_CATEGORY_COMMANDS,"Panic","panic"),
	settingCommandNameShoutout(SETTINGS_CATEGORY_COMMANDS,"Shoutout","so"),
	settingCommandNameSong(SETTINGS_CATEGORY_COMMANDS,"Song","song"),
	settingCommandNameTimezone(SETTINGS_CATEGORY_COMMANDS,"Timezone","timezone"),
	settingCommandNameUptime(SETTINGS_CATEGORY_COMMANDS,"Uptime","uptime"),
	settingCommandNameTotalTime(SETTINGS_CATEGORY_COMMANDS,"TotalTime","totaltime"),
	settingCommandNameVibe(SETTINGS_CATEGORY_COMMANDS,"Vibe","vibe"),
	settingCommandNameVibeVolume(SETTINGS_CATEGORY_COMMANDS,"VibeVolume","volume")
{
	DeclareCommand({settingCommandNameAgenda,"Set the agenda of the stream, displayed in the header of the chat window",CommandType::NATIVE,true},NativeCommandFlag::AGENDA);
	DeclareCommand({settingCommandNameStreamCategory,"Change the stream category",CommandType::NATIVE,true},NativeCommandFlag::CATEGORY);
	DeclareCommand({settingCommandNameStreamTitle,"Change the stream title",CommandType::NATIVE,true},NativeCommandFlag::TITLE);
	DeclareCommand({settingCommandNameCommands,"List all of the commands Celeste recognizes",CommandType::NATIVE,false},NativeCommandFlag::COMMANDS);
	DeclareCommand({settingCommandNameEmote,"Toggle emote only mode in chat",CommandType::NATIVE,true},NativeCommandFlag::EMOTE);
	DeclareCommand({settingCommandNameFollowage,"Show how long a user has followed the broadcaster",CommandType::NATIVE,false},NativeCommandFlag::FOLLOWAGE);
	DeclareCommand({settingCommandNameHTML,"Format the chat message as HTML",CommandType::NATIVE,false},NativeCommandFlag::HTML);
	DeclareCommand({settingCommandNamePanic,"Crash Celeste",CommandType::NATIVE,true},NativeCommandFlag::PANIC);
	DeclareCommand({settingCommandNameShoutout,"Call attention to another streamer's channel",CommandType::NATIVE,false},NativeCommandFlag::SHOUTOUT);
	DeclareCommand({settingCommandNameSong,"Show the title, album, and artist of the song that is currently playing",CommandType::NATIVE,false},NativeCommandFlag::SONG);
	DeclareCommand({settingCommandNameTimezone,"Display the timezone of the system the bot is running on",CommandType::NATIVE,false},NativeCommandFlag::TIMEZONE);
	DeclareCommand({settingCommandNameUptime,"Show how long the bot has been connected",CommandType::NATIVE,false},NativeCommandFlag::TOTAL_TIME);
	DeclareCommand({settingCommandNameTotalTime,"Show how many total hours stream has ever been live",CommandType::NATIVE,false},NativeCommandFlag::UPTIME);
	DeclareCommand({settingCommandNameVibe,"Start the playlist of music for the stream",CommandType::NATIVE,true},NativeCommandFlag::VIBE);
	DeclareCommand({settingCommandNameVibeVolume,"Adjust the volume of the vibe keeper",CommandType::NATIVE,true},NativeCommandFlag::VOLUME);
	LoadViewerAttributes();

	if (settingRoasts) LoadRoasts();
	LoadBadgeIconURLs();
	StartClocks();

	lastRaid=QDateTime::currentDateTime().addMSecs(static_cast<qint64>(0)-static_cast<qint64>(settingRaidInterruptDuration));

	connect(vibeKeeper,&Music::Player::Print,this,&Bot::Print);
	if (settingVibePlaylist) vibeKeeper->Load(settingVibePlaylist);
}

void Bot::DeclareCommand(const Command &&command,NativeCommandFlag flag)
{
	commands.insert({{command.Name(),command}});
	nativeCommandFlags.insert({{command.Name(),flag}});
}

QJsonDocument Bot::LoadDynamicCommands()
{
	QFile commandListFile(Filesystem::DataPath().filePath(COMMANDS_LIST_FILENAME));
	if (!commandListFile.exists())
	{
		if (!Filesystem::Touch(commandListFile)) throw std::runtime_error(QString(FILE_ERROR_TEMPLATE_COMMANDS_LIST_CREATE).arg(commandListFile.fileName()).toStdString());
	}
	if (!commandListFile.open(QIODevice::ReadOnly)) throw std::runtime_error(QString(FILE_ERROR_TEMPLATE_COMMANDS_LIST_OPEN).arg(commandListFile.fileName()).toStdString());

	QJsonParseError jsonError;
	QByteArray data=commandListFile.readAll();
	if (data.isEmpty()) data="[]";
	QJsonDocument json=QJsonDocument::fromJson(data,&jsonError);
	if (json.isNull()) throw std::runtime_error(QString(FILE_ERROR_TEMPLATE_COMMANDS_LIST_PARSE).arg(jsonError.errorString()).toStdString());

	return json;
}

Command::Lookup Bot::DeserializeCommands(const QJsonDocument &json)
{
	const QJsonArray objects=json.array();
	for (const QJsonValue &jsonValue : objects)
	{
		QJsonObject jsonObject=jsonValue.toObject();
		const QString name=jsonObject.value(JSON_KEY_COMMAND_NAME).toString();
		if (!commands.contains(name))
		{
			const QString type=jsonObject.value(JSON_KEY_COMMAND_TYPE).toString();
			if (!COMMAND_TYPES.contains(type))
			{
				emit Print(QString("Command type '%1' doesn't exist for command '%2'").arg(type,name));
				continue;
			}
			commands[name]={
				name,
				jsonObject.value(JSON_KEY_COMMAND_DESCRIPTION).toString(),
				COMMAND_TYPES.at(jsonObject.value(JSON_KEY_COMMAND_TYPE).toString()),
				jsonObject.contains(JSON_KEY_COMMAND_RANDOM_PATH) ? jsonObject.value(JSON_KEY_COMMAND_RANDOM_PATH).toBool() : false,
				jsonObject.value(JSON_KEY_COMMAND_PATH).toString(),
				jsonObject.contains(JSON_KEY_COMMAND_MESSAGE) ? jsonObject.value(JSON_KEY_COMMAND_MESSAGE).toString() : QString(),
				jsonObject.contains(JSON_KEY_COMMAND_PROTECTED) ? jsonObject.value(JSON_KEY_COMMAND_PROTECTED).toBool() : false
			};
		}
		if (jsonObject.contains(JSON_KEY_COMMAND_ALIASES))
		{
			const QJsonArray aliases=jsonObject.value(JSON_KEY_COMMAND_ALIASES).toArray();
			for (const QJsonValue &jsonValue : aliases)
			{
				const QString alias=jsonValue.toString();
				commands.try_emplace(alias,alias,&commands.at(name));
				if (commands.at(alias).Type() == CommandType::NATIVE) nativeCommandFlags.insert({alias,nativeCommandFlags.at(name)});
			}
		}
	}
	return commands;
}

QJsonDocument Bot::SerializeCommands(const Command::Lookup &entries)
{
	NativeCommandFlagLookup mergedNativeCommandFlags;
	QJsonArray array;
	std::unordered_map<QString,QStringList> aliases;
	for (const Command::Entry &entry : entries)
	{
		const Command &command=entry.second;

		if (command.Parent())
		{
			aliases[command.Parent()->Name()].push_back(command.Name());
			continue; // if this is just an alias, move on without processing it as a full command
		}

		QJsonObject object;
		object.insert(JSON_KEY_COMMAND_NAME,command.Name());

		QString type;
		switch (command.Type())
		{
		case CommandType::NATIVE:
			mergedNativeCommandFlags.insert(nativeCommandFlags.extract(command.Name()));
			continue;
		case CommandType::AUDIO:
			object.insert(JSON_KEY_COMMAND_TYPE,COMMAND_TYPE_AUDIO);
			object.insert(JSON_KEY_COMMAND_DESCRIPTION,command.Description());
			object.insert(JSON_KEY_COMMAND_PATH,command.Path());
			if (command.Random()) object.insert(JSON_KEY_COMMAND_RANDOM_PATH,true);
			if (!command.Message().isEmpty()) object.insert(JSON_KEY_COMMAND_MESSAGE,command.Message());
			if (command.Protected()) object.insert(JSON_KEY_COMMAND_PROTECTED,command.Protected());
			break;
		case CommandType::VIDEO:
			object.insert(JSON_KEY_COMMAND_TYPE,COMMAND_TYPE_VIDEO);
			object.insert(JSON_KEY_COMMAND_DESCRIPTION,command.Description());
			object.insert(JSON_KEY_COMMAND_PATH,command.Path());
			if (command.Random()) object.insert(JSON_KEY_COMMAND_RANDOM_PATH,true);
			if (command.Protected()) object.insert(JSON_KEY_COMMAND_PROTECTED,command.Protected());
			break;
		case CommandType::PULSAR:
			object.insert(JSON_KEY_COMMAND_TYPE,COMMAND_TYPE_PULSAR);
			object.insert(JSON_KEY_COMMAND_DESCRIPTION,command.Description());
			if (command.Protected()) object.insert(JSON_KEY_COMMAND_PROTECTED,command.Protected());
			break;
		}

		array.append(object);
	}

	for (QJsonValueRef value : array)
	{
		// if aliases exist for this object, attach the array of aliases to it
		QJsonObject object=value.toObject();
		QString name=object.value(JSON_KEY_COMMAND_NAME).toString();
		auto candidate=aliases.find(name);
		if (candidate != aliases.end())
		{
			QJsonArray names;
			QStringList nodes=aliases.extract(candidate).mapped();
			for (const QString &alias : nodes) names.append(alias);
			object.insert(JSON_KEY_COMMAND_ALIASES,names);
		}
		value=object;
	}

	// catch the aliases that were for native commands,
	// which normally aren't listed in the commands file
	for (const std::pair<QString,QStringList> &pair : aliases)
	{
		QJsonArray names;
		for (const QString &name : pair.second) names.append(name);
		array.append(QJsonObject({
			{JSON_KEY_COMMAND_NAME,pair.first},
			{JSON_KEY_COMMAND_ALIASES,names}
		}));
	}

	this->commands.swap(commands);
	nativeCommandFlags.swap(mergedNativeCommandFlags);

	return QJsonDocument(array);
}

bool Bot::SaveDynamicCommands(const QJsonDocument &json)
{
	QFile commandListFile(Filesystem::DataPath().filePath(COMMANDS_LIST_FILENAME));
	if (!commandListFile.open(QIODevice::WriteOnly))
	{
		emit Print(QString(FILE_ERROR_TEMPLATE_COMMANDS_LIST_OPEN).arg(commandListFile.fileName()),QStringLiteral("load dynamic commands"));
		return false;
	}
	commandListFile.write(json.toJson(QJsonDocument::Indented));
	return true;
}

const Command::Lookup& Bot::Commands() const
{
	return commands;
}

bool Bot::LoadViewerAttributes() // FIXME: have this throw an exception rather than return a bool
{
	QFile commandListFile(Filesystem::DataPath().filePath(VIEWER_ATTRIBUTES_FILENAME));
	if (!commandListFile.open(QIODevice::ReadWrite))
	{
		emit Print(QString("Failed to open user attributes file: %1").arg(commandListFile.fileName()));
		return false;
	}

	QJsonParseError jsonError;
	QByteArray data=commandListFile.readAll();
	if (data.isEmpty()) data="{}";
	QJsonDocument json=QJsonDocument::fromJson(data,&jsonError);
	if (json.isNull())
	{
		emit Print(jsonError.errorString());
		return false;
	}

	const QJsonObject entries=json.object();
	for (QJsonObject::const_iterator viewer=entries.begin(); viewer != entries.end(); ++viewer)
	{
		const QJsonObject attributes=viewer->toObject();
		viewers[viewer.key()]={
			attributes.contains(JSON_KEY_COMMANDS) ? attributes.value(JSON_KEY_COMMANDS).toBool() : true,
			attributes.contains(JSON_KEY_WELCOME) ? attributes.value(JSON_KEY_WELCOME).toBool() : false,
			attributes.contains(JSON_KEY_BOT) ? attributes.value(JSON_KEY_BOT).toBool() : false
		};
	}

	return true;
}

void Bot::SaveViewerAttributes(bool resetWelcomes)
{
	QFile commandListFile(Filesystem::DataPath().filePath(VIEWER_ATTRIBUTES_FILENAME));
	if (!commandListFile.open(QIODevice::ReadWrite)) return; // FIXME: how can we report the error here while closing?

	QJsonObject entries;
	for (const std::pair<QString,Viewer::Attributes> &viewer : viewers)
	{
		QJsonObject attributes={
			{JSON_KEY_COMMANDS,viewer.second.commands},
			{JSON_KEY_WELCOME,resetWelcomes ? false : viewer.second.welcomed},
			{JSON_KEY_BOT,viewer.second.bot}
		};
		entries.insert(viewer.first,attributes);
	}

	commandListFile.write(QJsonDocument(entries).toJson(QJsonDocument::Indented));
}

void Bot::LoadRoasts()
{
	connect(&roastSources,&QMediaPlaylist::loadFailed,this,[this]() {
		emit Print(QString("Failed to load roasts: %1").arg(roastSources.errorString()));
	});
	connect(&roastSources,&QMediaPlaylist::loaded,this,[this]() {
		roastSources.shuffle();
		roastSources.setCurrentIndex(Random::Bounded(0,roastSources.mediaCount()));
		roaster->setPlaylist(&roastSources);
		connect(&inactivityClock,&QTimer::timeout,roaster,&QMediaPlayer::play);
		emit Print("Roasts loaded!");
	});
	roastSources.load(QUrl::fromLocalFile(settingRoasts));
	connect(roaster,QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),this,[this](QMediaPlayer::Error error) {
		emit Print(QString("Roaster failed to start: %1").arg(roaster->errorString()));
	});
	connect(roaster,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status) {
		if (status == QMediaPlayer::EndOfMedia)
		{
			roaster->stop();
			roastSources.setCurrentIndex(Random::Bounded(0,roastSources.mediaCount()));
		}
	});
}

void Bot::LoadBadgeIconURLs()
{
	Network::Request({TWITCH_API_ENDPOINT_BADGES},Network::Method::GET,[](QNetworkReply *reply) {
		const char *JSON_KEY_ID="id";
		const char *JSON_KEY_SET_ID="set_id";
		const char *JSON_KEY_VERSIONS="versions";
		const char *JSON_KEY_IMAGE_URL="image_url_1x";

		const QJsonDocument json=QJsonDocument::fromJson(reply->readAll());
		if (json.isNull()) return;
		const QJsonObject object=json.object();
		if (object.isEmpty() || !object.contains(JSON_KEY_DATA)) return;
		const QJsonArray sets=object.value(JSON_KEY_DATA).toArray();
		for (const QJsonValue &set : sets)
		{
			const QJsonObject object=set.toObject();
			if (!object.contains(JSON_KEY_SET_ID) || !object.contains(JSON_KEY_VERSIONS)) continue;
			const QString setID=object.value(JSON_KEY_SET_ID).toString();
			const QJsonArray versions=object.value(JSON_KEY_VERSIONS).toArray();
			for (const QJsonValue &version : versions)
			{
				const QJsonObject object=version.toObject();
				if (!object.contains(JSON_KEY_ID) || !object.contains(JSON_KEY_IMAGE_URL)) continue;
				const QString setVersion=object.value(JSON_KEY_ID).toVariant().toString();
				const QString versionURL=object.value(JSON_KEY_IMAGE_URL).toString();
				badgeIconURLs[setID][setVersion]=versionURL;
			}
		}
	},{},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()}
	});
}

void Bot::StartClocks()
{
	inactivityClock.setInterval(TimeConvert::Interval(std::chrono::milliseconds(settingInactivityCooldown)));
	if (settingPortraitVideo)
	{
		connect(&inactivityClock,&QTimer::timeout,this,[this]() {
			emit PlayVideo(settingPortraitVideo);
		});
	}
	inactivityClock.start();

	helpClock.setInterval(TimeConvert::Interval(std::chrono::milliseconds(settingHelpCooldown)));
	connect(&helpClock,&QTimer::timeout,this,[this]() {
		std::unordered_map<QString,Command>::iterator candidate=std::next(std::begin(commands),Random::Bounded(commands));
		emit ShowCommand(candidate->second.Name(),candidate->second.Description());
	});
	helpClock.start();
}

void Bot::Ping()
{
	if (settingPortraitVideo)
		emit ShowPortraitVideo(settingPortraitVideo);
	else
		emit Print("Letting Twitch server know we're still here...");
}

void Bot::Redemption(const QString &name,const QString &rewardTitle,const QString &message)
{
	if (rewardTitle == "Crash Celeste")
	{
		DispatchPanic(name);
		vibeKeeper->Stop();
		return;
	}
	emit AnnounceRedemption(name,rewardTitle,message);
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

void Bot::SuppressMusic()
{
	vibeKeeper->DuckVolume(true);
}

void Bot::RestoreMusic()
{
	vibeKeeper->DuckVolume(false);
}

void Bot::DispatchArrival(const QString &login)
{
	if (viewers.contains(login))
	{
		if (viewers.at(login).bot || viewers.at(login).welcomed) return;
	}
	else
	{
		viewers[login]={};
	}

	if (static_cast<QString>(settingArrivalSound).isEmpty()) return; // this isn't an error; clearing the setting is how you turn arrival announcements off

	Viewer::Remote *viewer=new Viewer::Remote(security,login);
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
	connect(viewer,&Viewer::Remote::Recognized,viewer,[this](Viewer::Local viewer) {
		if (security.Administrator() != viewer.Name() && QDateTime::currentDateTime().toMSecsSinceEpoch()-lastRaid.toMSecsSinceEpoch() > static_cast<qint64>(settingRaidInterruptDuration))
		{
			Viewer::ProfileImage::Remote *profileImage=viewer.ProfileImage();
			connect(profileImage,&Viewer::ProfileImage::Remote::Retrieved,profileImage,[this,viewer](const QImage &profileImage) {
				emit AnnounceArrival(viewer.DisplayName(),profileImage,QFileInfo(settingArrivalSound).isDir() ? File::List(settingArrivalSound).Random() : settingArrivalSound);
				viewers.at(viewer.Name()).welcomed=true;
				SaveViewerAttributes(false);
			});
			connect(profileImage,&Viewer::ProfileImage::Remote::Print,this,&Bot::Print);
		}
	});
}

void Bot::ParseChatMessage(const QString &prefix,const QString &source,const QStringList &parameters,const QString &message)
{
	std::optional<QStringView> window;

	QStringView remainingText(message);
	QStringView login;
	Chat::Message chatMessage;

	// parse tags
	window=prefix;
	std::unordered_map<QStringView,QStringView> tags;
	while (!window->isEmpty())
	{
		std::optional<QStringView> pair=StringView::Take(*window,';');
		if (!pair) continue; // skip malformated badges rather than making a visible fuss
		std::optional<QStringView> key=StringView::Take(*pair,'=');
		if (!key) continue; // same as above
		std::optional<QStringView> value=StringView::Last(*pair,'=');
		if (!value) continue; // I'll rely on "missing" to represent an empty value
		tags[*key]=*value;
	}
	if (tags.contains(CHAT_TAG_DISPLAY_NAME)) chatMessage.sender=tags.at(CHAT_TAG_DISPLAY_NAME).toString();
	if (tags.contains(CHAT_TAG_COLOR)) chatMessage.color=tags.at(CHAT_TAG_COLOR).toString();

	// badges
	if (tags.contains(CHAT_TAG_BADGES))
	{
		std::unordered_map<QStringView,QStringView> badges;
		QStringView &versions=tags.at(CHAT_TAG_BADGES);
		while (!versions.isEmpty())
		{
			std::optional<QStringView> pair=StringView::Take(versions,',');
			if (!pair) continue;
			std::optional<QStringView> name=StringView::Take(*pair,'/');
			if (!name) continue;
			std::optional<QStringView> version=StringView::Last(*pair,'/');
			if (!version) continue; // a badge must have a version
			badges.insert({*name,*version});
			std::optional<QString> badgeIconPath=DownloadBadgeIcon(name->toString(),version->toString());
			if (!badgeIconPath) continue;
			chatMessage.badges.append(*badgeIconPath);
		}
		if (badges.contains(CHAT_BADGE_BROADCASTER) && badges.at(CHAT_BADGE_BROADCASTER) == u"1") chatMessage.broadcaster=true;
		if (badges.contains(CHAT_BADGE_MODERATOR) && badges.at(CHAT_BADGE_MODERATOR) == u"1") chatMessage.moderator=true;
	}

	// emotes
	if (tags.contains(CHAT_TAG_EMOTES))
	{
		QStringView &entries=tags.at(CHAT_TAG_EMOTES);
		while (!entries.isEmpty())
		{
			std::optional<QStringView> entry=StringView::Take(entries,'/');
			if (!entry) continue;
			std::optional<QStringView> id=StringView::Take(*entry,':');
			if (!id) continue;
			while(!entry->isEmpty())
			{
				std::optional<QStringView> occurrence=StringView::Take(*entry,',');
				std::optional<QStringView> left=StringView::First(*occurrence,'-');
				std::optional<QStringView> right=StringView::Last(*occurrence,'-');
				if (!left || !right) continue;
				unsigned int start=StringConvert::PositiveInteger(left->toString());
				unsigned int end=StringConvert::PositiveInteger(right->toString());
				const Chat::Emote emote {
					.id=id->toString(),
					.start=start,
					.end=end
				};
				chatMessage.emotes.push_back(emote);
			}
		}
		std::sort(chatMessage.emotes.begin(),chatMessage.emotes.end());
	}

	window=source;

	// hostmask
	// TODO: break these down in the Channel class, not here
	std::optional<QStringView> hostmask=StringView::Take(*window,' ');
	if (!hostmask) return;
	std::optional<QStringView> user=StringView::Take(*hostmask,'!');
	if (!user) return;
	login=*user;

	if (login == TWITCH_SYSTEM_ACCOUNT_NAME)
	{
		if (DispatchChatNotification(remainingText.toString())) return;
	}

	// determine if this is a command, and if so, process it as such
	// and if it's valid, we're done
	window=message;
	if (std::optional<QStringView> command=StringView::Take(*window,' '); command)
	{
		if (command->size() > 0 && command->at(0) == '!')
		{
			chatMessage.text=window->toString().trimmed();
			if (DispatchCommand(command->mid(1).toString(),chatMessage,login.toString())) return;
		}
	}

	if (!chatMessage.broadcaster) DispatchArrival(login.toString());

	// determine if the message is an action
	remainingText=remainingText.trimmed();
	if (const QString ACTION("\001ACTION"); remainingText.startsWith(ACTION))
	{
		remainingText=remainingText.mid(ACTION.size(),remainingText.lastIndexOf('\001')-ACTION.size()).trimmed();
		chatMessage.action=true;
	}

	// set emote name and check for wall of text
	int emoteCharacterCount=0;
	for (Chat::Emote &emote : chatMessage.emotes)
	{
		const QStringView name=remainingText.mid(emote.start,1+emote.end-emote.start); // end is an index, not a size, so we have to add 1 to get the size
		emoteCharacterCount+=name.size();
		emote.name=name.toString();
		DownloadEmote(emote); // once we know the emote name, we can determine the path, which means we can download it (download will set the path in the struct)
	}
	if (remainingText.size()-emoteCharacterCount > static_cast<int>(settingTextWallThreshold)) emit AnnounceTextWall(message,settingTextWallSound);

	chatMessage.text=remainingText.toString().toHtmlEscaped();
	emit ChatMessage(chatMessage);
	inactivityClock.start();
}

std::optional<QString> Bot::DownloadBadgeIcon(const QString &badge,const QString &version)
{
	if (!badgeIconURLs.contains(badge) || !badgeIconURLs.at(badge).contains(version)) return std::nullopt;
	QString badgeIconURL=badgeIconURLs.at(badge).at(version);
	static const QString CHAT_BADGE_ICON_PATH_TEMPLATE=Filesystem::TemporaryPath().filePath(QString("%1_%2.png"));
	QString badgePath=CHAT_BADGE_ICON_PATH_TEMPLATE.arg(badge,version);
	if (!QFile(badgePath).exists())
	{
		Network::Request(badgeIconURL,Network::Method::GET,[this,badgeIconURL,badgePath](QNetworkReply *downloadReply) {
			if (downloadReply->error())
			{
				emit Print(QString("Failed to download badge %1: %2").arg(badgeIconURL,downloadReply->errorString()));
				return;
			}
			if (!QImage::fromData(downloadReply->readAll()).save(badgePath)) emit Print(QString("Failed to save badge %2").arg(badgePath));
			emit RefreshChat();
		});
	}
	return badgePath;
}

void Bot::DownloadEmote(Chat::Emote &emote)
{
	emote.path=Filesystem::TemporaryPath().filePath(QString("%1.png").arg(emote.id));
	if (!QFile(emote.path).exists())
	{
		Network::Request(QString(TWITCH_API_ENDPOINT_EMOTE_URL).arg(emote.id),Network::Method::GET,[this,emote](QNetworkReply *downloadReply) {
			if (downloadReply->error())
			{
				emit Print(QString("Failed to download emote %1: %2").arg(emote.name,downloadReply->errorString()));
				return;
			}
			if (!QImage::fromData(downloadReply->readAll()).save(emote.path)) emit Print(QString("Failed to save emote %1 to %2").arg(emote.name,emote.path));
			emit RefreshChat();
		});
	}
}

bool Bot::DispatchCommand(const QString name,const Chat::Message &chatMessage,const QString &login)
{
	if (commands.find(name) == commands.end()) return false;
	Command command=chatMessage.text.isEmpty() ? commands.at(name) : Command(commands.at(name),chatMessage.text);

	if (command.Protected() && !chatMessage.Privileged())
	{
		emit AnnounceDeniedCommand(File::List(settingDeniedCommandVideo).Random());
		emit Print(QString(R"(The command "!%1" is protected but requester is not authorized")").arg(command.Name()));
		return false;
	}

	Viewer::Remote *viewer=new Viewer::Remote(security,login);
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
	connect(viewer,&Viewer::Remote::Recognized,viewer,[this,command,chatMessage](Viewer::Local viewer) {
		switch (command.Type())
		{
		case CommandType::VIDEO:
			DispatchVideo(command);
			break;
		case CommandType::AUDIO:
			emit PlayAudio(viewer.DisplayName(),command.Message(),command.Path());
			break;
		case CommandType::PULSAR:
			emit Pulse(command.Message());
			break;
		case CommandType::NATIVE:
			switch (nativeCommandFlags.at(command.Name()))
			{
			case NativeCommandFlag::AGENDA:
				emit SetAgenda(command.Message());
				break;
			case NativeCommandFlag::CATEGORY:
				StreamCategory(command.Message());
				break;
			case NativeCommandFlag::COMMANDS:
				DispatchCommandList();
				break;
			case NativeCommandFlag::EMOTE:
				ToggleEmoteOnly();
				break;
			case NativeCommandFlag::FOLLOWAGE:
				DispatchFollowage(viewer.Name());
				break;
			case NativeCommandFlag::HTML:
				emit ChatMessage({
					.sender=chatMessage.sender,
					.text=chatMessage.text,
					.color=chatMessage.color,
					.badges=chatMessage.badges,
					.emotes={},
					.action=chatMessage.action,
					.broadcaster=chatMessage.broadcaster,
					.moderator=chatMessage.moderator
				});
				break;
			case NativeCommandFlag::PANIC:
				DispatchPanic(viewer.DisplayName());
				break;
			case NativeCommandFlag::SHOUTOUT:
				DispatchShoutout(command);
				break;
			case NativeCommandFlag::SONG:
				emit ShowCurrentSong(vibeKeeper->SongTitle(),vibeKeeper->AlbumTitle(),vibeKeeper->AlbumArtist(),vibeKeeper->AlbumCoverArt());
				break;
			case NativeCommandFlag::TIMEZONE:
				emit ShowTimezone(QDateTime::currentDateTime().timeZone().displayName(QDateTime::currentDateTime().timeZone().isDaylightTime(QDateTime::currentDateTime()) ? QTimeZone::DaylightTime : QTimeZone::StandardTime,QTimeZone::LongName));
				break;
			case NativeCommandFlag::TITLE:
				StreamTitle(command.Message());
				break;
			case NativeCommandFlag::TOTAL_TIME:
				DispatchUptime(true);
				break;
			case NativeCommandFlag::UPTIME:
				DispatchUptime(false);
				break;
			case NativeCommandFlag::VIBE:
				ToggleVibeKeeper();
				break;
			case NativeCommandFlag::VOLUME:
				AdjustVibeVolume(command);
				break;
			}
			break;
		case CommandType::BLANK:
			break;
		};
	});

	return true;
}

bool Bot::DispatchChatNotification(const QStringView &message)
{
	if (message.contains(u"is now hosting you."))
	{
		std::optional<QStringView> host=StringView::First(message,' ');
		if (!host) return false;
		emit AnnounceHost(host->toString(),settingHostSound);
		return true;
	}
	return false;
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
	// TODO: Modify File::List to accept a filter and use it
	QDir directory(command.Path());
	QStringList videos=directory.entryList(QStringList() << "*.mp4",QDir::Files);
	if (videos.size() < 1)
	{
		emit Print("No videos found");
		return;
	}
	emit PlayVideo(directory.absoluteFilePath(videos.at(Random::Bounded(videos))));
}

void Bot::DispatchCommandList()
{
	std::vector<std::tuple<QString,QStringList,QString>> descriptions;
	for (const std::pair<const QString,Command> &pair : commands)
	{
		const Command &command=pair.second;
		if (command.Parent() || command.Protected()) continue;
		QStringList aliases;
		const std::vector<Command*> &children=command.Children();
		for (const Command *child : children) aliases.append(child->Name());
		descriptions.push_back({command.Name(),aliases,command.Description()});
	}
	emit ShowCommandList(descriptions);
}

void Bot::DispatchFollowage(const QString &name)
{
	Viewer::Remote *viewer=new Viewer::Remote(security,name);
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
	connect(viewer,&Viewer::Remote::Recognized,viewer,[this](Viewer::Local viewer) {
		Network::Request({TWITCH_API_ENDPOING_USER_FOLLOWS},Network::Method::GET,[this,viewer](QNetworkReply *reply) {
			QJsonParseError jsonError;
			QJsonDocument json=QJsonDocument::fromJson(reply->readAll(),&jsonError);
			if (json.isNull() || !json.object().contains("data"))
			{
				emit Print("Something went wrong obtaining stream information");
				return;
			}
			QJsonArray data=json.object().value("data").toArray();
			if (data.size() < 1)
			{
				emit Print("Response from requesting stream information was incomplete ");
				return;
			}
			QJsonObject details=data.at(0).toObject();
			QDateTime start=QDateTime::fromString(details.value("followed_at").toString(),Qt::ISODate);
			std::chrono::milliseconds duration=static_cast<std::chrono::milliseconds>(start.msecsTo(QDateTime::currentDateTimeUtc()));
			std::chrono::years years=std::chrono::duration_cast<std::chrono::years>(duration);
			std::chrono::months months=std::chrono::duration_cast<std::chrono::months>(duration-years);
			std::chrono::days days=std::chrono::duration_cast<std::chrono::days>(duration-years-months);
			emit ShowFollowage(viewer.DisplayName(),years,months,days);
		},{
			{"from_id",viewer.ID()},
			{"to_id",security.AdministratorID()}
		},{
			{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
			{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
			{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
		});
	});
}

void Bot::DispatchPanic(const QString &name)
{
	QFile outputFile(Filesystem::DataPath().absoluteFilePath("panic.txt"));
	QString outputText;
	if (!outputFile.open(QIODevice::ReadOnly)) return;
	outputText=QString(outputFile.readAll()).arg(name);
	outputFile.close();
	QString date=QDateTime::currentDateTime().toString("ddd d hh:mm:ss");
	outputText=outputText.split("\n").join(QString("\n%1 ").arg(date));
	emit Panic(date+"\n"+outputText);
}

void Bot::DispatchShoutout(Command command)
{
	Viewer::Remote *viewer=new Viewer::Remote(security,QString(command.Message()).remove("@"));
	connect(viewer,&Viewer::Remote::Recognized,viewer,[this](Viewer::Local viewer) {
		Viewer::ProfileImage::Remote *profileImage=viewer.ProfileImage();
		connect(profileImage,&Viewer::ProfileImage::Remote::Retrieved,profileImage,[this,viewer](const QImage &profileImage) {
			emit Shoutout(viewer.DisplayName(),viewer.Description(),profileImage);
		});
	});
	connect(viewer,&Viewer::Remote::Print,this,&Bot::Print);
}

void Bot::DispatchUptime(bool total)
{
	Network::Request({TWITCH_API_ENDPOINT_STREAM_INFORMATION},Network::Method::GET,[this,total](QNetworkReply *reply) {
		QJsonParseError jsonError;
		QJsonDocument json=QJsonDocument::fromJson(reply->readAll(),&jsonError);
		if (json.isNull() || !json.object().contains("data"))
		{
			emit Print("Something went wrong obtaining stream information");
			return;
		}
		QJsonArray data=json.object().value("data").toArray();
		if (data.size() < 1)
		{
			emit Print("Response from requesting stream information was incomplete ");
			return;
		}
		QJsonObject details=data.at(0).toObject();
		QDateTime start=QDateTime::fromString(details.value("started_at").toString(),Qt::ISODate);
		std::chrono::milliseconds duration=static_cast<std::chrono::milliseconds>(start.msecsTo(QDateTime::currentDateTimeUtc()));
		if (total) duration+=std::chrono::minutes(static_cast<qint64>(settingUptimeHistory));
		std::chrono::hours hours=std::chrono::duration_cast<std::chrono::hours>(duration);
		std::chrono::minutes minutes=std::chrono::duration_cast<std::chrono::minutes>(duration-hours);
		std::chrono::seconds seconds=std::chrono::duration_cast<std::chrono::seconds>(duration-hours-minutes);
		if (total)
			emit ShowTotalTime(hours,minutes,seconds);
		else
			emit ShowUptime(hours,minutes,seconds);
	},{
		{"user_login",security.Administrator()}
	},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
		{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
	});
}

void Bot::ToggleVibeKeeper()
{
	if (vibeKeeper->Playing())
	{
		emit Print("Pausing the vibes...");
		vibeKeeper->Stop();
		return;
	}
	vibeKeeper->Start();
}

void Bot::AdjustVibeVolume(Command command)
{
	try
	{
		QStringView window(command.Message());
		std::optional<QStringView> targetVolume=StringView::Take(window,' ');
		if (!targetVolume) throw std::runtime_error("Target volume is missing");
		if (window.size() < 1) throw std::runtime_error("Duration for volume change is missing");
		vibeKeeper->Volume(StringConvert::PositiveInteger(targetVolume->toString()),std::chrono::seconds(StringConvert::PositiveInteger(window.toString())));
	}

	catch (const std::runtime_error &exception)
	{
		emit Print(QString("%1: %2").arg("Failed to adjust volume",exception.what()));
	}
}

void Bot::ToggleEmoteOnly()
{
	Network::Request({TWITCH_API_ENDPOINT_CHAT_SETTINGS},Network::Method::GET,[this](QNetworkReply *reply) {
		QJsonParseError jsonError;
		QJsonDocument json=QJsonDocument::fromJson(reply->readAll(),&jsonError);
		if (json.isNull() || !json.object().contains("data"))
		{
			emit Print("Something went wrong changing emote only mode");
			return;
		}
		QJsonArray data=json.object().value("data").toArray();
		if (data.size() < 1)
		{
			emit Print("Response was incomplete changing emote only mode");
			return;
		}
		QJsonObject details=data.at(0).toObject();
		if (details.value("emote_mode").toBool())
			EmoteOnly(false);
		else
			EmoteOnly(true);
	},{
		{QUERY_PARAMETER_BROADCASTER_ID,security.AdministratorID()},
		{QUERY_PARAMETER_MODERATOR_ID,security.AdministratorID()}
	},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
		{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
	});
}

void Bot::EmoteOnly(bool enable)
{
	// just use broadcasterID for both
	// this is for authorizing the moderator at the API-level, which we're not worried about here
	// it's always going to be the broadcaster running the bot and access is protected through
	// DispatchCommand()
	Network::Request({TWITCH_API_ENDPOINT_CHAT_SETTINGS},Network::Method::PATCH,[this](QNetworkReply *reply) {
		if (reply->error()) emit Print(QString("Something went wrong setting emote only: %1").arg(reply->errorString()));
	},{
		{QUERY_PARAMETER_BROADCASTER_ID,security.AdministratorID()},
		{QUERY_PARAMETER_MODERATOR_ID,security.AdministratorID()}
	},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
		{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
	},QJsonDocument(QJsonObject({{"emote_mode",enable}})).toJson(QJsonDocument::Compact));
}

void Bot::StreamTitle(const QString &title)
{
	Network::Request({TWITCH_API_ENDPOINT_CHANNEL_INFORMATION},Network::Method::PATCH,[this,title](QNetworkReply *reply) {
		if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 204)
		{
			emit Print("Failed to change stream title");
			return;
		}
		emit Print(QString(R"(Stream title changed to "%1")").arg(title));
	},{
		{QUERY_PARAMETER_BROADCASTER_ID,security.AdministratorID()}
	},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
		{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
	},
	{
		QJsonDocument(QJsonObject({{"title",title}})).toJson(QJsonDocument::Compact)
	});
}

void Bot::StreamCategory(const QString &category)
{
	Network::Request({TWITCH_API_ENDPOINT_GAME_INFORMATION},Network::Method::GET,[this,category](QNetworkReply *reply) {
		QJsonParseError jsonError;
		QJsonDocument json=QJsonDocument::fromJson(reply->readAll(),&jsonError);
		if (json.isNull() || !json.object().contains("data"))
		{
			emit Print("Something went wrong finding stream category");
			return;
		}
		QJsonArray data=json.object().value("data").toArray();
		if (data.size() < 1)
		{
			emit Print("Response was incomplete finding stream category");
			return;
		}
		Network::Request({TWITCH_API_ENDPOINT_CHANNEL_INFORMATION},Network::Method::PATCH,[this,category](QNetworkReply *reply) {
			if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 204)
			{
				emit Print("Failed to change stream category");
				return;
			}
			emit Print(QString(R"(Stream category changed to "%1")").arg(category));
		},{
			{QUERY_PARAMETER_BROADCASTER_ID,security.AdministratorID()}
		},{
			{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
			{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
			{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
		},
		{
			QJsonDocument(QJsonObject({{"game_id",data.at(0).toObject().value("id").toString()}})).toJson(QJsonDocument::Compact)
		});
	},{
		{"name",category}
	},{
		{NETWORK_HEADER_AUTHORIZATION,StringConvert::ByteArray(QString("Bearer %1").arg(static_cast<QString>(security.OAuthToken())))},
		{NETWORK_HEADER_CLIENT_ID,security.ClientID()},
		{Network::CONTENT_TYPE,Network::CONTENT_TYPE_JSON},
	});
}

ApplicationSetting& Bot::ArrivalSound()
{
	return settingArrivalSound;
}

ApplicationSetting& Bot::PortraitVideo()
{
	return settingPortraitVideo;
}