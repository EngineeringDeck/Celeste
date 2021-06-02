#include <QtTest/QtTest>
#include <iostream>
#include "subscribers.h"
#include "window.h"

class TestEventSubscriber : public EventSubscriber
{
public:
	TestEventSubscriber(QObject *parent=nullptr) : EventSubscriber(parent) { }

	void TestDataAvailable()
	{
		DataAvailable();
	}

	void Data(const QByteArray &data) { this->data=data; }
protected:
	QByteArray data;

	QTcpSocket* SocketFromSignalSender() const override
	{
		return nullptr;
	}

	QByteArray ReadFromSocket(QTcpSocket *socket=nullptr) const override
	{
		return data;
	}

	bool WriteToSocket(const QByteArray &data,QTcpSocket *socket=nullptr) const override
	{
		Print(Console::GenerateMessage(SUBSYSTEM_NAME,"write",data));
		return true;
	}
};

class TestWindow : public Window
{
public:
	TestWindow() : Window(), validResponse(true) { }

	void TestDataAvailable()
	{
		DataAvailable();
	}

	void BreakResponses() { validResponse=false; }

	TestEventSubscriber* EventSubscriber() const
	{
		return testEventSubscriber;
	}
protected:
	bool validResponse;
	TestEventSubscriber *testEventSubscriber;

	QByteArray ReadFromSocket() const override
	{
		return validResponse ? ircSocket->readAll() : QByteArray("Things");
	}

	class EventSubscriber* CreateEventSubscriber() override
	{
		testEventSubscriber=new TestEventSubscriber(this);
		return testEventSubscriber;
	}
};

class Testing : public QObject
{
	Q_OBJECT
public:
	void DataPath(const QString &path)
	{
		dataPath.setPath(path);
	}

	QByteArray DataFromFile(const QString &filename)
	{
		QFile file(dataPath.filePath(filename));
		if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Failed to open data file");
		QByteArray data=file.readAll();
		file.close();
		return data;
	}
private:
	TestWindow testWindow;
	QDir dataPath;
private slots:
	void initTestCase()
	{
		testWindow.show();
	}

	void badSubscriptionDataTest()
	{
		testWindow.EventSubscriber()->Data(DataFromFile("channel_follow.txt"));
		testWindow.EventSubscriber()->TestDataAvailable();
	}

	void verifyConnected()
	{
		QSignalSpy windowSpy(&testWindow,&TestWindow::GreenLight);
		QVERIFY(windowSpy.wait(30000));
	}

	void multipleMessagesTest()
	{
		testWindow.BreakResponses();
		testWindow.TestDataAvailable();
		QVERIFY(true);
	}
};

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "ERROR: Please specify path to test data." << std::endl;
		return 1;
	}

	Testing testing;
	testing.DataPath(argv[1]);

	argc=1;
	QApplication app(argc,argv);
	return QTest::qExec(&testing,argc,argv);
}
#include "testing.moc"
