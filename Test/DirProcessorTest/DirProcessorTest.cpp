#include <QtTest>
#include "DirProcessor.h"
#include "ThumbsProviderInterface.h"
#include "ThumbGenerators/VideoThumbnailGeneratorInterface.h"
#include "ThumbGenerators/VideoThumbnailGeneratorFactory.h"

class MockThumbsProvider : public ThumbsProviderInterface {
public:
    QImage getImage(const QString &id) const override
    {
        getImageCount++;
        return {};
    }
    void addImage(const QString path, const QImage &image) override
    {
        addImageCount++;
    }
    bool haveThumb(const QString path) const override
    {
        haveThumbCount++;
        return true;
    }

    mutable int getImageCount = 0;
    mutable int addImageCount = 0;
    mutable int haveThumbCount = 0;
    void clear(){
        getImageCount = 0;
        addImageCount = 0;
        haveThumbCount = 0;
    };
};

class FakeThumbGenerator : public VideoThumbnailGeneratorInterface {
    bool getThumbs(QString path, int count, QImage images[]) override {return true;}
    void abort() override {}
};
class FakeThumbGeneratorFactory : public VideoThumbnailGeneratorFactory {
    VideoThumbnailGeneratorInterface* createGenerator() {return new FakeThumbGenerator();}
};

class DirProcessorTest : public QObject
{
    Q_OBJECT

public:
    DirProcessorTest();
    ~DirProcessorTest();
    MockThumbsProvider provider;
    FakeThumbGeneratorFactory factory;
    DirProcessor dirProcessor;
private slots:
    void initTestCase();
    void cleanupTestCase();
    void getUnitedDirFiles();

};

DirProcessorTest::DirProcessorTest() : dirProcessor(&provider, factory)
{

}

DirProcessorTest::~DirProcessorTest()
{

}

void DirProcessorTest::initTestCase()
{

}

void DirProcessorTest::cleanupTestCase()
{

}

void DirProcessorTest::getUnitedDirFiles()
{
    QString pathToData = TEST_DATADIR;
    QList<ThumbData> thumbsOut;
    bool allHaveThumbs = true;
    dirProcessor.getUnitedDirFiles({pathToData + "dir1/", pathToData + "dir2/"},
                                   thumbsOut, &allHaveThumbs);
    QList<ThumbData> expected = {
        {"a.jpg", pathToData + "dir1/a.jpg"},
        {"b.jpg", pathToData + "dir2/b.jpg"},
        {"c.jpg", pathToData + "dir1/c.jpg"},
        {"d.jpg", pathToData + "dir2/d.jpg"},
    };
    QCOMPARE(expected, thumbsOut);
    QCOMPARE(expected.size(), provider.haveThumbCount);

}

QTEST_APPLESS_MAIN(DirProcessorTest)

#include "DirProcessorTest.moc"
