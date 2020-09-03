#include <map>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QElapsedTimer>

#include "krunner_bridge.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

KRunnerBridge::KRunnerBridge(QObject* parent, const QVariantList& args)
  : Plasma::AbstractRunner(parent, args)
{
    meta = metadata(Plasma::RunnerReturnPluginMetaDataConstant());

    QString cwd = QDir::cleanPath(QDir::homePath() + QDir::separator() + ".local/share/kservices5");
    if(!QDir(cwd).exists())
        cwd = ".";

    for (const QString& kw : meta.rawData().keys()) {
        if (!kw.startsWith("X-KRunner-Bridge-Script"))
            continue;

        QString script = meta.value(kw, "");
        if(script.isEmpty())
            continue;

        // find the corresponding script in desktop file install path
        QString abs_script = QDir::cleanPath(cwd + QDir::separator() + script);
        if(QFileInfo(abs_script).exists())
            script = abs_script;

        workers.push_back(new KRunnerBridgeWorker(cwd, script));

        connect(this, &Plasma::AbstractRunner::prepare, workers.back(), &KRunnerBridgeWorker::prepare);

        workers.back()->start();
    }

    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setDefaultSyntax(Plasma::RunnerSyntax(
        QString::fromLatin1(":q:"), meta.description()));
}

KRunnerBridge::~KRunnerBridge()
{
    for (int i = 0; i < workers.size(); ++i) {
        if (workers[i])
            workers[i]->deleteLater();
    }
}

void KRunnerBridgeWorker::prepare()
{
    if (!process) {
        process = new QProcess(this);
    }

    if (process->state() == QProcess::NotRunning) {
        process->setWorkingDirectory(workingDirectory);
        process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        process->start("sh", QStringList() << "-c" << script);
    }

    QJsonObject command;
    command.insert("operation", QJsonValue("init"));

    //qDebug().nospace() << "(" << script << ") <- " << command;
    QJsonValue ret = invoke(command);
    //qDebug().nospace() << "(" << script << ") -> " << ret;
}

QJsonValue KRunnerBridgeWorker::invoke(const QJsonObject& doc)
{
    Q_ASSERT(responseBuffer.isEmpty());

    if (!process || process->state() == QProcess::NotRunning)
        return QJsonValue();

    const int timeout = 1000;

    responseBuffer.clear();

    QElapsedTimer timer;
    timer.start();

    // Discard all pending input
    if (process->isReadable())
        process->readAll();

    process->write(QJsonDocument(doc).toJson(QJsonDocument::Compact).append('\n'));

    while (timer.elapsed() < timeout) {
        process->waitForReadyRead(timeout - timer.elapsed());
        responseBuffer.append(process->readAll());

        int lineFeed = responseBuffer.indexOf('\n');

        if (lineFeed != -1) {
            responseBuffer.remove(lineFeed, responseBuffer.size() - lineFeed);
            QJsonDocument ret = QJsonDocument::fromJson(responseBuffer);
            responseBuffer.clear();

            return ret.isObject() ? ret.object() : QJsonValue();
        }
    }

    qDebug() << script << "was terminated for timeout";

    process->terminate();
    process->waitForFinished();

    return QJsonValue();
}

void KRunnerBridgeWorker::invokeAsync(QJsonObject command)
{
    //qDebug().nospace() << "(" << script << ") <- " << command;
    QJsonValue ret = invoke(command);
    //qDebug().nospace() << "(" << script << ") -> " << ret;

    emit invokeAsyncReturned(ret);
}

QVector<QJsonValue> KRunnerBridge::invokeAll(const QJsonObject& command)
{
    // This function is required to be locked to prevent return value pollution.
    QMutexLocker locker(&invokeMutex);

    QEventLoop waitLoop;

    QVector<QJsonValue> responses;
    int workersCount = workers.size();
    responses.resize(workersCount);

    for (int i = 0; i < workers.size(); ++i) {
        waitLoop.connect(workers[i], &KRunnerBridgeWorker::invokeAsyncReturned, &waitLoop,
            [&responses, &waitLoop, &workersCount, i](QJsonValue v) {
                responses[i] = v;
                workersCount--;
                if (workersCount == 0)
                    waitLoop.quit();
            });

        QMetaObject::invokeMethod(workers[i], "invokeAsync", Q_ARG(QJsonObject, command));
    }

    waitLoop.exec();

    return responses;
}

void KRunnerBridge::match(Plasma::RunnerContext& ctxt)
{
    if (!ctxt.isValid()) return;

    QJsonObject command;
    command.insert("operation", QJsonValue("match"));
    command.insert("query", QJsonValue(ctxt.query()));

    QVector<QJsonValue> responses = invokeAll(command);

    for (int i = 0; i < responses.size(); ++i) {
        const QString& script = workers[i]->script;
        const QJsonValue& doc = responses[i];

        if (doc.isNull())
            continue;

        KB_ASSERT(doc.isObject());
        QJsonValue v_result = doc.toObject().value("result");
        KB_ASSERT(v_result.isArray());
        QJsonArray results = v_result.toArray();

        for(QJsonValue result : results) {
            KB_ASSERT(result.isObject());
            QJsonObject obj = result.toObject();

            Plasma::QueryMatch m(this);

#define MAP_PROPERTY(t, camel1, camel2) \
            QJsonValue camel2 = obj.value(#camel2); \
            if(camel2.is##t()) m.set##camel1(camel2.to##t());

            MAP_PROPERTY(Double, Relevance, relevance);
            MAP_PROPERTY(String, Text, text);
            MAP_PROPERTY(String, Subtext, subtext);
            MAP_PROPERTY(String, MatchCategory, matchCategory);
            MAP_PROPERTY(String, IconName, iconName);
            MAP_PROPERTY(String, MimeType, mimeType);

#undef MAP_PROPERTY

            QJsonValue data = obj.value("data");
            if(!data.isUndefined()) m.setData(data.toVariant());

            if(relevance == 1)
                m.setType(Plasma::QueryMatch::ExactMatch);
            else m.setType(Plasma::QueryMatch::CompletionMatch);

            ctxt.addMatch(m);
        }
    }
}

void KRunnerBridge::run(const Plasma::RunnerContext& ctxt, const Plasma::QueryMatch& match)
{
    Q_UNUSED(ctxt);

    QJsonObject command;
    command.insert("operation", QJsonValue("run"));
    command.insert("data", QJsonValue::fromVariant(match.data()));

    invokeAll(command);
}

K_EXPORT_PLASMA_RUNNER(krunner_bridge, KRunnerBridge)

#include "krunner_bridge.moc"
