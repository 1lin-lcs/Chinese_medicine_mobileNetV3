#include "chinese_medicine_server.h"

Chinese_Medicine_Server::Chinese_Medicine_Server()
{}

/*! \brief 这是读取配置文件用的
 *  \note 默认读取目录下的configuration.json文件
 *  \return true/false 读取成功就返回true，否则false
*/
bool Chinese_Medicine_Server::ReadConfig(){
    QFile configFile(QDir::currentPath()+"/configuration.json");
    if(!configFile.open(QIODevice::ReadOnly)){
        qInfo()<<"打开配置文件失败";
        return false;
    }
    QJsonParseError jsonerror;
    QJsonDocument doc(QJsonDocument::fromJson(configFile.readAll(),&jsonerror));
    if(doc.isNull()||jsonerror.error!=QJsonParseError::NoError){
        qInfo()<<"读取配置文件失败";
        qInfo()<<jsonerror.errorString();
        return false;
    }
    QJsonObject top=doc.object();
    QJsonObject config=top.value("Config").toObject();
    QJsonObject userInfo=top.value("DataBaseUser").toObject();

    Serverconfig.IsAutoQuit=config.value("IsAutoQuit").toBool();
    Serverconfig.InvervalTime=config.value("InvervalTime").toInt();
    Serverconfig.QuitCoint=config.value("QuitCount").toInt();
#ifdef UseC++
    LoadModule(config.value("ModelPath").toString());
    categoryNum=config.value("CategoryNum").toInt();
    LoadCategories(config.value("CategorisePath").toString());
#endif

    Info.HostName=userInfo.value("HostName").toString();
    Info.Port=userInfo.value("Port").toInt();
    Info.DataBaseName=userInfo.value("DataBaseName").toString();
    Info.UserName=userInfo.value("UserName").toString();
    Info.PassWord=userInfo.value("Password").toString();
    return true;
}

/*! \brief 这是启动服务，进行监听，并连接数据库
 *  \note 确保ReadConfig()函数有运行且成功，否则出现错误
 *  \return true/false，启动成功就是true，否则false
*/
bool Chinese_Medicine_Server::Start(){
    server=new MyTcpServer();
    if(!server->listen(QHostAddress::LocalHost,50002)){
        qInfo()<<"Server启动失败";
        return false;
    }
    connect(server,&MyTcpServer::SocketDesc,this,&Chinese_Medicine_Server::CreateSocket);
    qInfo()<<"Server启动成功";
    DataBase=new MyDataBase(Info.HostName,Info.Port,Info.DataBaseName,Info.UserName,Info.PassWord);
    if(!DataBase->LinkDB("User")){                                               //起个名字而已，实际作用是关闭用的
        qInfo()<<"连接数据库失败";
        return false;
    }
    if(Serverconfig.IsAutoQuit){
        if(Serverconfig.InvervalTime>0)
            Timer.start(Serverconfig.InvervalTime*1000);
        else
            Timer.start(10*1000);
        connect(&Timer,&QTimer::timeout,this,&Chinese_Medicine_Server::CloseProgram);
    }
    return true;
}

/*! \brief 这是处理Json文件内容的函数
 *  \param doc 从socket发来的Json文件的指针
 *  \param socketdesc socket的描述符
 *  \note 注意及时清除指针的内容
*/
void Chinese_Medicine_Server::Task(QJsonDocument *doc, qintptr socketdesc){
    QJsonObject obj=doc->object();
    QJsonObject status;
    if(!obj.contains("status")){
        CreateErrorJsonInfo(socketdesc,"信息缺失");
        delete doc;
        return;
    }
    status=obj.value("status").toObject();
    if(status.value("error").toInt()!=0){
        delete doc;
        return;
    }
    switch(status.value("client").toInt()){
    case 0:TaskSignIn(doc,socketdesc);break;
    case 1:TaskSignUp(doc,socketdesc);break;
    case 2:ChangePasswd(doc,socketdesc);break;
    case 3:DeleteUser(doc,socketdesc);break;
    case 4:TaskIdentify(doc,socketdesc);break;
    default:CreateErrorJsonInfo(socketdesc,"事件错误");delete doc;break;
    }
}

/*! \brief 处理登录事件
 *  \param doc Json信息
 *  \param socketdesc socket的描述字符
*/
void Chinese_Medicine_Server::TaskSignIn(QJsonDocument *doc, qintptr socketdesc){
    QJsonObject information=doc->object();
    if(!information.contains("information")){
        CreateErrorJsonInfo(socketdesc,"缺失information键");
        delete doc;
        return;
    }
    information=information.value("information").toObject();

    QString user=information.value("username").toString();
    QString passwd=information.value("password").toString();

    QString sql1=QString("select username from %1 where username=\"%2\"").arg(UserTable,user);
    if(DataBase->FindSingleData(sql1)==""){
        CreateErrorJsonInfo(socketdesc,"无此用户");
        delete doc;
        return;
    }

    QString sql2=QString("select password from %1 where username=\"%2\"").arg(UserTable,user);
    QString res=DataBase->FindSingleData(sql2);

    if(res==""){
        CreateErrorJsonInfo(socketdesc,"没有设置密码");
        delete doc;
        return;
    }

    if(res.compare(passwd)!=0){
        CreateErrorJsonInfo(socketdesc,"密码错误");
        delete doc;
        return;
    }

    CreateSuccessJsonInfo(socketdesc,0,"获得账户信息成功，可以登录");
    delete doc;
}

/*! \brief 处理注册的事件
 *  \param doc Json文档的内容
 *  \param socketdesc socket的描述符
*/
void Chinese_Medicine_Server::TaskSignUp(QJsonDocument *doc, qintptr socketdesc){
    QJsonObject information=doc->object();
    if(!information.contains("information")){
        CreateErrorJsonInfo(socketdesc,"内容缺失");
        delete doc;
        return;
    }
    information=information.value("information").toObject();

    QString username=information.value("username").toString();
    QString sql1=QString("select username from %1 where username=\"%2\"").arg(UserTable,username);
    if(DataBase->FindSingleData(sql1)!=""){
        CreateErrorJsonInfo(socketdesc,"用户已存在");
        delete doc;
        return;
    }

    QString passwd=information.value("password").toString();
    QString email=information.value("email").toString();
    QString sql2=QString("insert into %1 (username,password,email,create_time) values ('%2','%3','%4',now())").arg(UserTable,username,passwd,email);
    if(DataBase->InsertData(sql2)){
        CreateSuccessJsonInfo(socketdesc,1,"注册成功");
        delete doc;
        return;
    }
    else{
        CreateErrorJsonInfo(socketdesc,"注册失败，服务端的问题");
        delete doc;
        return;
    }
}

/*! \brief 处理修改密码事件
 *  \param doc Json文档的内容
 *  \param socketdesc socket的描述符
*/
void Chinese_Medicine_Server::ChangePasswd(QJsonDocument* doc, qintptr socketdesc){
    QJsonObject information=doc->object();
    if(!information.contains("information")){
        CreateErrorJsonInfo(socketdesc,"内容缺失");
        delete doc;
        return;
    }

    information=information.value("information").toObject();

    QString user=information.value("username").toString();
    QString sql1=QString("select username from %1 where username=\"%2\"").arg(UserTable,user);
    if(DataBase->FindSingleData(sql1)==""){
        CreateErrorJsonInfo(socketdesc,"用户不存在");
        delete doc;
        return;
    }

    QString passwd=information.value("password").toString();
    QString sql2=QString("select password from %1 where username=\"%2\"").arg(UserTable,user);
    QString res=DataBase->FindSingleData(sql2);
    if(passwd!=res){
        CreateErrorJsonInfo(socketdesc,"原密码错误");
        delete doc;
        return;
    }
    QString sql3=QString("update %1 set password=\"%2\" where username = \"%3\"").arg(UserTable,passwd,user);
    if(DataBase->UpdataData(sql3)){
        CreateSuccessJsonInfo(socketdesc,2,"修改成功");
        delete doc;
        return;
    }
    else{
        CreateErrorJsonInfo(socketdesc,"修改失败，服务端的问题");
        delete doc;
        return;
    }
}

/*! \brief 处理注销用户事件
 *  \param doc Json文档的内容
 *  \param socketdesc socket的描述符
*/
void Chinese_Medicine_Server::DeleteUser(QJsonDocument *doc, qintptr socketdesc){
    QJsonObject information=doc->object();
    if(!information.contains("information")){
        CreateErrorJsonInfo(socketdesc,"内容缺失");
        delete doc;
        return;
    }

    information=information.value("information").toObject();
    QString user=information.value("username").toString();
    QString sql1=QString("select username from %1 where username=\"%2\"").arg(UserTable,user);
    if(DataBase->FindSingleData(sql1)==""){
        CreateErrorJsonInfo(socketdesc,"用户不存在");
        delete doc;
        return;
    }

    QString sql2=QString("delete from %1 where username=\"%2\"").arg(UserTable,user);
    if(DataBase->DeleteData(sql2)){
        CreateSuccessJsonInfo(socketdesc,3,"注销成功");
        delete doc;
        return;
    }
    else{
        CreateErrorJsonInfo(socketdesc,"注销失败，服务端的问题");
        delete doc;
        return;
    }
}

/*! \brief 处理图片识别事件
 *  \param doc Json文档的内容
 *  \param socketdesc socket的描述符
*/
void Chinese_Medicine_Server::TaskIdentify(QJsonDocument *doc, qintptr socketdesc){
    QJsonObject response=doc->object();
    if(!response.contains("response")){
        CreateErrorJsonInfo(socketdesc,"内容缺失");
        delete doc;
        return;
    }
    response=response.value("response").toObject();
    QByteArray img=QByteArray::fromBase64(response.value("data").toVariant().toByteArray());
    QString filetype=response.value("external").toString();
    QString path = QDir::currentPath() + "/tmp/" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + filetype;
    QDir dir;
    if(!dir.exists(QDir::currentPath()+"/tmp")){
        dir.mkdir(QDir::currentPath()+"/tmp");
    }
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly)){
        qInfo()<<"打开文件失败";
        return;
    }
    file.write(img);
    file.close();

    delete doc;

#ifdef UsePython
    IdentityThread* thread = new IdentityThread(path,QDir::currentPath(),socketdesc);
    connect(thread, &IdentityThread::SendEnd,this, &Chinese_Medicine_Server::ReceiveEnd);
    connect(thread, &IdentityThread::SendError, this,&Chinese_Medicine_Server::GetThreadError);
    thread->start();
#endif

#ifdef UseC++
    IdentityThreadCpp* thread=new IdentityThreadCpp(path,categories,module,socketdesc);
    connect(thread,&IdentityThreadCpp::SendResult,this,&Chinese_Medicine_Server::ReceiverResult);
    thread->start();
#endif
}

/*! \brief 生成含有错误信息的Json内容
 *  \param socketdesc socket的描述字符
 *  \param info 写入response的内容
*/
void Chinese_Medicine_Server::CreateErrorJsonInfo(qintptr socketdesc,QString info){
    QJsonObject top,status,response;

    status.insert("error",1);

    response.insert("data",info);

    top.insert("status",status);
    top.insert("response",response);

    QJsonDocument* doc=new QJsonDocument(top);
    emit SendJsonDoc(doc,socketdesc);
}

/*! \brief 生成成功处理后的Json内容
 *  \param socketdesc socket的描述字符
 *  \param event 事件类型
 *  \param info 写入response的内容
*/
void Chinese_Medicine_Server::CreateSuccessJsonInfo(qintptr socketdesc, int event, QString info){
    QJsonObject top,status,response;

    status.insert("server",event);
    status.insert("error",0);

    response.insert("data",info);

    top.insert("status",status);
    top.insert("response",response);

    QJsonDocument* doc=new QJsonDocument(top);
    emit SendJsonDoc(doc,socketdesc);
}


#ifdef UsePython

/*! \brief 如果使用的调用Python进行识别，这个函数就是初始化Python环境
 *  \return true/false 成功初始化Python环境为true，否则false
*/
bool Chinese_Medicine_Server::InitPython(){
    Py_Initialize();
    if(!Py_IsInitialized()){
        qInfo()<<"Python环境启动失败";
        return false;
    }
    PyEval_InitThreads();
    int nInit=PyEval_ThreadsInitialized();
    if(nInit){
        PyEval_SaveThread();
    }
    return true;
}
#endif

#ifdef UseC++
/*!
 * \brief 加载植物种类
 * \param filepath 种类的路径
 * \return 成功加载返回true，否则false
 */
bool Chinese_Medicine_Server::LoadCategories(QString filepath){
    QFile file(filepath);
    if(!file.open(QIODevice::ReadOnly)){
        qInfo()<<"打开类别文件失败";
        return false;
    }
    QJsonParseError jsonerror;
    QJsonDocument doc(QJsonDocument::fromJson(file.readAll(),&jsonerror));
    if(jsonerror!=QJsonParseError::NoError){
        qInfo()<<"读取类别文件出错";
        qInfo()<<jsonerror.errorString();
        return false;
    }
    if(i==0){
        qInfo()<<"没有种类";
        return false;
    }
    QJsonObject category=doc.object();
    for(int i=0;i<=categoryNum;i++){
        categories.append(category.value(QString::number(i)).toString());
    }
    return true;
}
#endif


//normal function
//-----------------------------------------------------------------------------------
//slot function


/*! \brief 自动退出程序
 *  \attention 这个函数还不完善
*/
void Chinese_Medicine_Server::CloseProgram(){
    if((Serverconfig.QuitCoint--)<0){
        if(server!=nullptr&&server->isListening()){
            server->close();
            delete server;
        }
        if(DataBase!=nullptr){
            DataBase->CloseDB();
            delete DataBase;
        }

        QDir path(QDir::currentPath() + "/tmp");
        path.removeRecursively();

#ifdef UsePython
        PyGILState_Ensure();
        Py_Finalize();
#endif

        exit(0);
    }
}

/*! \brief 创建新的socket并移入新线程处理
*/
void Chinese_Medicine_Server::CreateSocket(qintptr socketdesc){
    MyTcpSocket* socket=new MyTcpSocket(socketdesc);

    connect(socket,&MyTcpSocket::SendJsonFile,this,&Chinese_Medicine_Server::GetJsonFile);
    connect(socket,&MyTcpSocket::readyRead,socket,&MyTcpSocket::GetJsonFile);
    connect(this,&Chinese_Medicine_Server::SendJsonDoc,socket,&MyTcpSocket::SendData);
    connect(socket,&MyTcpSocket::disconnected,this,&Chinese_Medicine_Server::DeleteSocketThread,Qt::QueuedConnection);

    QThread* thread=new QThread(this);
    socket->moveToThread(thread);

    QHash<MyTcpSocket*,QThread*> map;
    map.insert(socket,thread);
    socketMap.insert(socketdesc,map);
}

/*! \brief 接受从socket传来的Json文件
 *  \param doc 从socket发来的Json文件的指针
 *  \param socketdesc socket的描述符
*/
void Chinese_Medicine_Server::GetJsonFile(/*QJsonDocument* */QJsonDocument *doc, qintptr socketdesc){
    Task(doc,socketdesc);
}

/*! \brief 删除没有连接的socket和thread
*/
void Chinese_Medicine_Server::DeleteSocketThread(){
    MyTcpSocket* socket=(MyTcpSocket*)sender();
    qintptr handle=socket->GetDesc();
    QHash temp=socketMap.value(handle);
    QThread* thread=temp.value(socket);
    socket->deleteLater();
    thread->deleteLater();
    socketMap.remove(handle);
}

#ifdef UsePython
/*! \brief 发送识别结果
*/
void Chinese_Medicine_Server::ReceiveEnd(QString end, qintptr socketdesc){
    CreateSuccessJsonInfo(socketdesc,4,end);
}

/*! \brief 打印识别错误
*/
void Chinese_Medicine_Server::GetThreadError(QString error){
    qInfo()<<error;
}
#endif

#ifdef UseC++
/*!
 * \brief 加载模型
 * \param filepath 模型的路径
 */
void Chinese_Medicine_Server::LoadModule(QString filepath){
    deviceType=torch::cuda::is_available()?torch::kCUDA:torch::kCPU;
    module=torch::jit::load(filepath,deviceType);
}

/*!
 * \brief 接受线程发过来的数据
 * \param end 结果
 * \param socketdesc 连接字节套
 */
void Chinese_Medicine_Server::ReceiverResult(QString end,qintptr socketdesc){
    CreateSuccessJsonInfo(socketdesc,4,end);
}
#endif
