#include "sql.h"

#include <QStringList>
#include <QVariant>
#include <QUuid>
#include <QDir>
#include <QDebug>

#if QT_VERSION >= 0x050000
#include <QStandardPaths>
#else
#include <QDesktopServices>
#endif

QString newGUID(QString table, QString column) {
    qDebug() << "newGUID()";
    if (table.isEmpty() || column.isEmpty())
        return QUuid::createUuid().toString().mid(1,36);

    for (QString uuid = QUuid::createUuid().toString().mid(1,36);; uuid = QUuid::createUuid().toString().mid(1,36)) {        
        QSqlQuery query;
        query.prepare(QString("SELECT COUNT(*) FROM %1 WHERE %2=:key").arg(table).arg(column));
        query.bindValue(":key", uuid);
        query.exec();

        if (!query.next())
            return "";

        if (query.value(0).toInt() == 0)
            return uuid;
    }
    return "";
}

QString AddSQLArray(int count)
{
    if (count <= 0)
        return QString();

    QString result = ":value0";
    for (int i=1; i<count; i++)
        result+= QString(", :value%1").arg(i);

    return result;
}

void BindSQLArray(QSqlQuery &query, QStringList values)
{
    for (int i=0; i<values.count(); i++)
        query.bindValue(QString(":value%1").arg(i), values.at(i));
}

sql::sql(QObject *parent) :
    QObject(parent)
{
#if QT_VERSION >= 0x050000
    QString dataDir = QStandardPaths::standardLocations(QStandardPaths::DataLocation).at(0);
#else
    QString dataDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif

    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    db = QSqlDatabase::addDatabase("QSQLITE");
    if (!db.isValid())
        qDebug() << "DB Klaida";
    db.setDatabaseName(dataDir + QDir::separator() + "db.sql");
    db.open();
}

sql::~sql()
{
    db.close();
}

void sql::dropTables()
{
    db.exec("DROP TABLE notes");
    db.exec("DROP TABLE notesTags");
    db.exec("DROP TABLE notebooks");
    db.exec("DROP TABLE tags");
    db.exec("DROP TABLE resources");
    checkTables();
}

void sql::updateSyncStatus(QString key, QVariant value)
{
    QSqlQuery query;
    query.prepare("REPLACE INTO syncStatus (option, value) VALUES (:key, :value)");
    query.bindValue(":key", key);
    query.bindValue(":value", value);
    query.exec();
}

QVariant sql::readSyncStatus(QString key, QVariant defaultValue)
{
    QSqlQuery query;
    query.prepare("SELECT value FROM syncStatus WHERE option=:key");
    query.bindValue(":key", key);
    query.exec();
    if (query.next())
        return query.value(0);

    return defaultValue;
}

void sql::checkTables()
{
    QString notesTable("CREATE TABLE notes ( ");
    notesTable +="guid VARCHAR(36) NOT NULL PRIMARY KEY, ";
    notesTable +="title TEXT, ";
    notesTable +="contentHash VARCHAR(16), ";
    notesTable +="created UNSIGNED BIG INT DEFAULT 0, ";
    notesTable +="updated UNSIGNED BIG INT DEFAULT 0, ";
    notesTable +="deleted UNSIGNED BIG INT DEFAULT 0, ";
    notesTable +="active BOOLEAN DEFAULT true, ";
    notesTable +="updateSequenceNum INTEGER DEFAULT 0, ";
    notesTable +="notebookGuid VARCHAR(36) NOT NULL";
    notesTable +=")";
    if (!db.tables().contains("notes"))
        db.exec(notesTable);

    QString conflictingNotes("CREATE TABLE conflictingNotes ( ");
    conflictingNotes +="guid VARCHAR(36) NOT NULL PRIMARY KEY, ";
    conflictingNotes +="contentHash VARCHAR(16), ";
    conflictingNotes +="updated UNSIGNED BIG INT DEFAULT 0 ";
    conflictingNotes +=")";
    if (!db.tables().contains("conflictingNotes"))
        db.exec(conflictingNotes);

    QString notesContent("CREATE TABLE notesContent ( ");
    notesContent +="hash VARCHAR(32) NOT NULL PRIMARY KEY,";
    notesContent +="content TEXT, ";
    notesContent +="length INTEGER DEFAULT 0";
    notesContent +=")";
    if (!db.tables().contains("notesContent"))
        db.exec(notesContent);

    QString notesTagsTable("CREATE TABLE notesTags ( ");
    notesTagsTable +="id INTEGER PRIMARY KEY AUTOINCREMENT, ";
    notesTagsTable +="noteGuid VARCHAR(36) NOT NULL, ";
    notesTagsTable +="guid VARCHAR(36) NOT NULL,";
    notesTagsTable +="UNIQUE(noteGuid, guid) ON CONFLICT REPLACE )";
    if (!db.tables().contains("notesTags"))
        db.exec(notesTagsTable);

    QString notebooksTable("CREATE TABLE notebooks ( ");
    notebooksTable +="guid VARCHAR(36) NOT NULL PRIMARY KEY, ";
    notebooksTable +="name TEXT, ";
    notebooksTable +="updateSequenceNum INTEGER DEFAULT -1, ";
    notebooksTable +="defaultNotebook BOOLEAN DEFAULT FALSE, ";
    notebooksTable +="serviceCreated UNSIGNED BIG INT DEFAULT 0, ";
    notebooksTable +="serviceUpdated UNSIGNED BIG INT DEFAULT 0, ";
    notebooksTable +="stack TEXT )";
    if (!db.tables().contains("notebooks"))
        db.exec(notebooksTable);

    QString tagsTable("CREATE TABLE tags ( ");
    tagsTable +="guid VARCHAR(36) NOT NULL PRIMARY KEY, ";
    tagsTable +="name TEXT, ";
    tagsTable +="parentGuid VARCHAR(36), ";
    tagsTable +="updateSequenceNum INTEGER DEFAULT -1 )";
    if (!db.tables().contains("tags"))
        db.exec(tagsTable);

    QString resourcesTable("CREATE TABLE resources ( ");
    resourcesTable +="guid VARCHAR(36) NOT NULL PRIMARY KEY, ";
    resourcesTable +="noteGuid VARCHAR(36), ";
    resourcesTable +="bodyHash VARCHAR(32), ";
    resourcesTable +="width INTEGER DEFAULT 0, ";
    resourcesTable +="height INTEGER DEFAULT 0, ";
    resourcesTable +="new BOOLEAN DEFAULT FALSE, ";
    resourcesTable +="updateSequenceNum INTEGER DEFAULT -1, ";
    resourcesTable +="size INTEGER DEFAULT 0, ";
    resourcesTable +="mime VARCHAR(255), ";
    resourcesTable +="fileName TEXT, ";
    resourcesTable +="sourceURL TEXT, ";
    resourcesTable +="attachment BOOLEAN DEFAULT FALSE, ";
    resourcesTable +="UNIQUE(noteGuid, bodyHash) ON CONFLICT REPLACE )";
    if (!db.tables().contains("resources"))
        db.exec(resourcesTable);

    QString dataTable("CREATE TABLE resourcesData ( ");
    dataTable +="hash VARCHAR(32) NOT NULL PRIMARY KEY, ";
    dataTable +="data BLOB)";
    if (!db.tables().contains("resourcesData"))
        db.exec(dataTable);

    QString syncStatusTable("CREATE TABLE syncStatus ( ");
    syncStatusTable +="option TEXT NOT NULL PRIMARY KEY, ";
    syncStatusTable +="value UNSIGNED BIG INT DEFAULT 0 )";
    if (!db.tables().contains("syncStatus"))
        db.exec(syncStatusTable);

    QString noteUpdates("CREATE TABLE noteUpdates ( ");
    noteUpdates += "id INTEGER PRIMARY KEY AUTOINCREMENT, ";
    noteUpdates += "guid VARCHAR(36) NOT NULL, ";
    noteUpdates += "field INT DEFAULT 0,";
    noteUpdates += "UNIQUE(guid, field) ON CONFLICT REPLACE )";
    if (!db.tables().contains("noteUpdates")) {
        db.exec(noteUpdates);
    }

    QString noteAttributes("CREATE TABLE noteAttributes ( ");
    noteAttributes += "id INTEGER PRIMARY KEY AUTOINCREMENT, ";
    noteAttributes += "noteGuid VARCHAR(36) NOT NULL, ";
    noteAttributes += "field TEXT,";
    noteAttributes += "value TEXT,";
    noteAttributes += "UNIQUE(noteGuid, field) ON CONFLICT REPLACE )";
    if (!db.tables().contains("noteAttributes")) {
        db.exec(noteAttributes);
    }

    QString tagsUpdates("CREATE TABLE tagsUpdates ( ");
    tagsUpdates += "id INTEGER PRIMARY KEY AUTOINCREMENT, ";
    tagsUpdates += "guid VARCHAR(36) NOT NULL, ";
    tagsUpdates += "field INT DEFAULT 0,";
    tagsUpdates += "UNIQUE(guid, field) ON CONFLICT REPLACE )";
    if (!db.tables().contains("tagsUpdates")) {
        db.exec(tagsUpdates);
    }

    QString notebookUpdates("CREATE TABLE notebookUpdates ( ");
    notebookUpdates += "id INTEGER PRIMARY KEY AUTOINCREMENT, ";
    notebookUpdates += "guid VARCHAR(36) NOT NULL, ";
    notebookUpdates += "field INT DEFAULT 0,";
    notebookUpdates += "UNIQUE(guid, field) ON CONFLICT REPLACE )";
    if (!db.tables().contains("notebookUpdates")) {
        db.exec(notebookUpdates);
    }
}
