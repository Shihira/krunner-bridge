#ifndef KRUNNER_BRIDGE
#define KRUNNER_BRIDGE

#include <QProcess>
#include <QDir>
#include <KF5/KRunner/krunner/abstractrunner.h>
#include "krunner_script.h"

class KRunnerBridge : public Plasma::AbstractRunner {
Q_OBJECT

    QList<KRunnerScript> scripts;
    QString cwd = QDir::cleanPath(QDir::homePath() + QDir::separator() + QStringLiteral(".local/share/kservices5"));
    int matchTimeout = 1000;
    int runTimeout = 2000;
    int scriptCount;
    // Hard coded JSON commands
    const QByteArray json_init = R"({"operation":"init"})";
    const QByteArray json_prepare = R"({"operation":"prepare"})";
    const QByteArray json_teardow = R"({"operation":"teardown"})";

public:
    KRunnerBridge(QObject *parent, const QVariantList &args);

    void match(Plasma::RunnerContext &) override;

    void run(const Plasma::RunnerContext &, const Plasma::QueryMatch &) override;

protected Q_SLOTS:

    void prepareForMatchSession();

    void matchSessionFinished();
};

#endif // KRUNNER_BRIDGE
