//  

#include "ExceptionSafeCoreApp.h"
#include "rkflogging/Logging.h"

namespace {
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "ExceptionSafeCoreApp")
}

ExceptionSafeCoreApp::ExceptionSafeCoreApp(int &argc, char **argv)
  : QCoreApplication(argc, argv){}

bool ExceptionSafeCoreApp::notify(QObject *obj, QEvent *event){
    // Cache the meta-object for error logging, in case the object is deleted
    const QMetaObject *const mObj = obj->metaObject();
    try{
        return QCoreApplication::notify(obj, event);
    }
    catch(std::exception &e){
        logError(obj, mObj, event, e.what());
    }
    catch(...){
        logError(obj, mObj, event, "Unknown exception");
    }

    return false;
}

void ExceptionSafeCoreApp::logError(const QObject *obj, const QMetaObject *mObj, const QEvent *event, const QString &msg) const{
    QString targetObj;
    if(obj){
        targetObj = QString("0x%1").arg(quint64(obj), 0, 16);
    }
    else{
        targetObj = "NULL";
    }
    QString targetClass;
    if(mObj){
        targetClass = mObj->className();
    }
    else{
        targetClass = "QObject";
    }

    QString eType;
    if(event){
        eType = QString::number(event->type());
    }
    else{
        eType = "UNKNOWN";
    }

    LOGGER_ERROR(logger) <<
        QString("Failed sending event type %1 to %2(%3): %4")
            .arg(eType).arg(targetClass, targetObj).arg(msg);
}

int ExceptionSafeCoreApp::exec(){
    LOGGER_INFO(logger) << "Entering event loop";
    const int status = QCoreApplication::exec();
    LOGGER_INFO(logger) << "Finished event loop";
    return status;
}
