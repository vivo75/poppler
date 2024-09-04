#include <QtTest/QtTest>

#include "PDFDoc.h"
#include "GlobalParams.h"

#include "Outline.h"
#include "poppler-private.h"

class TestUtf8Document : public QObject
{
    Q_OBJECT
public:
    explicit TestUtf8Document(QObject *parent = nullptr) : QObject(parent) { }
private Q_SLOTS:
    void checkStrings();
};

inline QString outlineItemTitle(OutlineItem *item)
{
    if (!item) {
        return {};
    }
    return QString::fromUcs4(item->getTitle(), item->getTitleLength());
}

void TestUtf8Document::checkStrings()
{

    globalParams = std::make_unique<GlobalParams>();
    auto doc = std::make_unique<PDFDoc>(std::make_unique<GooString>(TESTDATADIR "/unittestcases/pdf20-utf8-test.pdf"));
    QVERIFY(doc);
    QVERIFY(doc->isOk());

    QVERIFY(doc->getOptContentConfig() && doc->getOptContentConfig()->hasOCGs());

    QCOMPARE(Poppler::UnicodeParsedString(doc->getDocInfoTitle().get()), QStringLiteral("表ポあA鷗ŒéＢ逍Üßªąñ丂㐀𠀀"));

    QSet<QString> expectedNames { QStringLiteral("گچپژ"), QStringLiteral("Layer 1") };
    QSet<QString> foundNames;

    for (auto &[ref, group] : doc->getOptContentConfig()->getOCGs()) {
        foundNames.insert(Poppler::UnicodeParsedString(group->getName()));
    }
    QCOMPARE(expectedNames, foundNames);

    auto outlineItems = doc->getOutline()->getItems();
    QVERIFY(outlineItems);
    QCOMPARE(outlineItems->size(), 3);

    QCOMPARE(outlineItemTitle(outlineItems->at(0)), QStringLiteral("PDF 2.0 with UTF-8 test file"));
    QCOMPARE(outlineItemTitle(outlineItems->at(1)), QStringLiteral("\u202A\u202Atest\u202A"));
    QCOMPARE(outlineItemTitle(outlineItems->at(2)), QStringLiteral("🌈️\n" /*emoji rainbow flag*/));
}

QTEST_GUILESS_MAIN(TestUtf8Document)

#include "check_utf8document.moc"
