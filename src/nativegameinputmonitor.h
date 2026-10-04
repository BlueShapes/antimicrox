#ifndef NATIVEGAMEINPUTMONITOR_H
#define NATIVEGAMEINPUTMONITOR_H

#include "nativegameinputstate.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

class NativeGameInputMonitor : public QObject
{
    Q_OBJECT
  public:
    explicit NativeGameInputMonitor(QObject *parent = nullptr);
    ~NativeGameInputMonitor() override;
    bool isSuspended() const { return m_suspended; }
    bool isListening() const { return server.isListening(); }
    QString serverName() const { return server.serverName(); }
    static QString endpointName();

  public slots:
    void setEnabled(bool enabled);
    void close();

  signals:
    void suspensionChanged(bool suspended);

  protected:
    virtual quintptr foregroundWindow() const;
    virtual quint64 uptimeMs() const;

  private:
    struct Connection
    {
        quint64 id;
        quint32 pid;
        quint64 connected;
        bool initialized = false;
        QByteArray buffer;
        void *process = nullptr;
    };
    void acceptConnections();
    void readConnection(QLocalSocket *socket);
    void removeConnection(QLocalSocket *socket);
    void recalculate();
    static bool windowOwner(quintptr window, quint32 pid);
    QLocalServer server;
    QTimer timer;
    QHash<QLocalSocket *, Connection> connections;
    NativeGameInputState state;
    quint64 nextConnection = 0;
    bool m_suspended = false;
    bool recalculating = false;
};

#endif
