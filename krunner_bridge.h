#ifndef KRUNNER_BRIDGE
#define KRUNNER_BRIDGE

#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <KF5/KRunner/krunner/abstractrunner.h>

class KRunnerBridgeWorker : public QThread {
    Q_OBJECT

    QProcess* process;
    QByteArray responseBuffer;

public:
    const QString workingDirectory;
    const QString script;

    KRunnerBridgeWorker(const QString& cwd, const QString& s)
        : process(nullptr)
        , workingDirectory(cwd)
        , script(s) { moveToThread(this); }

    void run() override { exec(); }

    void prepare();

    QJsonValue invoke(const QJsonObject& doc);

public slots:
    void invokeAsync(QJsonObject doc);

signals:
    void invokeAsyncReturned(QJsonValue ret);
};

class KRunnerBridge : public Plasma::AbstractRunner {
    Q_OBJECT

    KPluginMetaData meta;

    QList<KRunnerBridgeWorker*> workers;
    QMutex invokeMutex;

public:
    KRunnerBridge(QObject* parent, const QVariantList& args);
    ~KRunnerBridge();

    void match(Plasma::RunnerContext&) override;
    void run(const Plasma::RunnerContext&, const Plasma::QueryMatch&) override;

private:
    QVector<QJsonValue> invokeAll(const QJsonObject& command);
};

#endif // KRUNNER_BRIDGE
