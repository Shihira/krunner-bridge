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
    bool debugOutput;
    // Timeouts
    int initTimeout;
    int matchTimeout;
    int runTimeout;
    int setupTimeout;
    int teardownTimeout;
    // Hard coded JSON commands
    const QByteArray json_init = R"({"operation":"init"})";
    const QByteArray json_prepare = R"({"operation":"prepare"})";
    const QByteArray json_teardown = R"({"operation":"teardown"})";

public:
    KRunnerBridge(QObject *parent, const QVariantList &args);

    void initializeValues();

    void parseKRunnerConfigLines();

    void parseKRunnerConfigGroups();

    void match(Plasma::RunnerContext &) override;

    void run(const Plasma::RunnerContext &, const Plasma::QueryMatch &) override;

protected Q_SLOTS:

    void prepareForMatchSession();

    void matchSessionFinished();
};

#endif // KRUNNER_BRIDGE
