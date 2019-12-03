#ifndef KRUNNER_BRIDGE
#define KRUNNER_BRIDGE

#include <QtCore/QProcess>
#include <KF5/KRunner/krunner/abstractrunner.h>

class KRunnerBridge : public Plasma::AbstractRunner {
Q_OBJECT

    QStringList scripts;
    QString cwd;
    int matchTimeout = 1000;
    int runTimeout = 2000;

    const QByteArray json_init = R"({"operation":"init"})";

public:
    KRunnerBridge(QObject *parent, const QVariantList &args);

    void match(Plasma::RunnerContext &) override;

    void run(const Plasma::RunnerContext &, const Plasma::QueryMatch &) override;
};

#endif // KRUNNER_BRIDGE
