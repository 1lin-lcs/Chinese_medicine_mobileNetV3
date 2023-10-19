#include "mydatabase.h"

MyDataBase::MyDataBase(QObject *parent)
    : QObject{parent}
{

}

/** @brief 这是用来初始化要连接数据库的信息
* @param hostname 数据库所在地址
* @param port 数据库端口
* @param databaseName 连接的数据库的名称
* @param username 登录用户的名称
* @param password 登录用户的密码
*/
MyDataBase::MyDataBase(QString hostname,int port,QString databaseName,QString username,QString password){
    HostName=hostname;
    Port=port;
    DataBaseName=databaseName;
    UserName=username;
    PassWord=password;
}

/** @brief 连接数据库
 *  @return true/false 连接成功返回true，否则false
*/
bool MyDataBase::LinkDB(){
    db=QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(HostName);
    db.setPort(Port);
    db.setDatabaseName(DataBaseName);
    db.setUserName(UserName);
    db.setPassword(PassWord);
    bool ok=db.open();
    if(!ok){
        qDebug()<<"打开数据库失败";
        return false;
    }
    qDebug()<<"连接数据库成功";
    return true;
}

/** @brief 插入数据到数据库
* @param sqlcommand 执行插入语句的内容，具体语句由调用者生成
* @return true/false 插入成功返回true，否则false
*/
bool MyDataBase::InsertData(QString sqlcommand){
    if(!db.open()){
        qDebug()<<"数据库未打开";
        return false;
    }
    QSqlQuery* query=new QSqlQuery();
    if(!query->exec(sqlcommand)){
        qDebug()<<"插入数据失败";
        delete query;
        return false;
    }
    delete query;
    return true;
}

/** @brief 查找数据，只能查到单个数据
 *  @param sqlcommand 执行查找语句的内容，具体语句由调用者生成
 *  @return data 查找到则返回数据，否则返回空字符串
*/
QString MyDataBase::FindSingleData(QString sqlcommand){
    if(!db.open()){
        qDebug()<<"数据库未打开";
        return "";
    }
    QSqlQuery* query=new QSqlQuery();
    if(!query->exec(sqlcommand)){
        qDebug()<<"查到数据失败";
        delete query;
        return "";
    }
    query->next();
    QString data=query->value(0).toString();
    delete query;
    return data;
}

/** @brief 查找数据，返回所有查到的数据
 *  @param  sqlcommand 执行查找语句的内容，具体语句由调用者生成
 *  @return data 所有数据的列表的指针，失败则返回空指针
*/
QList<QStringList>* MyDataBase::FindDatas(QString sqlcommand){
    if(!db.open()){
        qDebug()<<"数据库未打开";
        return nullptr;
    }
    QSqlQuery* query=new QSqlQuery();
    if(!query->exec(sqlcommand)){
        qDebug()<<"查找数据失败";
        delete query;
        return nullptr;
    }
    QSqlRecord record=query->record();
    int count=record.count();
    QStringList values;
    QList<QStringList> data=new QList<QStringL>;
    while(query->next()){
        for(int i=0;i<count;i++){
            values.append(query->value(i).toString());
        }
        data.append(values);
    }
    return data;
}

/** @brief 更新数据
 *  @param sqlcommand 执行更新语句的内容，具体语句由调用者生成
 *  @return true/false 更新成功返回true，否则false
*/
bool MyDataBase::UpdataData(QString sqlcommand){
    if(!db.open()){
        qDebug()<<"数据库未打开";
        return false;
    }
    QSqlQuery* query=new QSqlQuery();
    if(!query->exec(sqlcommand)){
        qDebug()<<"更新数据失败";
        delte query;
        return false;
    }
    delete query;
    return true;
}

/** @brief 删除数据，注意这是删除一行数据的
 *  @param sqlcommand 执行删除语句的内容，具体语句由调用者生成
 *  @return true/false 删除成功返回true，否则false
*/
bool MyDataBase::DeleteData(QString sqlcommand){
    if(!db.open()){
        qDebug()<<"数据库未打开";
        return false;
    }
    QSqlQuery* query=new QSqlQuery;
    if(!query->exec(sqlcommand)){
        qDebug()<<"删除数据失败";
        delete query;
        return false;
    }
    delete query;
    return true;
}

/** @brief 返回所连接数据库所有表的名称
 *  @return names 所有表的名称
*/
QStringList* MyDataBase::GetDataTableName(){
    QString sqlcommand=QString("show tables");
    QSqlQuery* query=new QSqlQuery();
    if(!query->exec(sqlcommand)){
        return nullptr;
    }
    QStringList names=new QStringList;
    while(query->next()){
        names.append(query->value(0).toString());
    }
    return names;
}
