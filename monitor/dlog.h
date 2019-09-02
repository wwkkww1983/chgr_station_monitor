#ifndef DLOG_H
#define DLOG_H
#include <qapplication.h>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QtMsgHandler>
#include <QMessageLogContext>
#include <QMutex>
 

void myMsgOutput(QtMsgType type, const QMessageLogContext &context, const QString& msg);
 
#endif // DLOG_H
