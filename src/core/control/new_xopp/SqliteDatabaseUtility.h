#pragma once

#include "control/model/Document.h"  // for Document
#include "filesystem.h"  // for path

class SqliteDatabaseUtility
{

public:
    SqliteDatabaseUtility();
    virtual ~SqliteDatabaseUtility();

public:
    /**
     * @brief Apre una connessione al database SQLite specificato.
     * @param dbPath Il percorso del file del database.
     * @return true se l'apertura ha successo, altrimenti false.
     */
    auto openDatabase(sqlite3* db, const std::string& dbPath) -> bool

    /**
     * @brief Crea lo schema del database eseguendo la query di creazione.
     * @return true se la creazione ha successo, altrimenti false.
     */
    auto createSchema() -> bool;

    /**
     * @brief Restituisce l'ultimo messaggio di errore registrato.
     * @return Una stringa contenente il messaggio di errore.
     */
    auto getErrorMessage() const -> std::string;

    std::unique_ptr<Document> loadDocument(fs::path const& filepath);

    std::string getLastError();

private:
    /**
     * @brief Esegue una query SQL generica sul database aperto.
     * @param sql La stringa della query SQL da eseguire.
     * @param errorMsg Un messaggio di errore personalizzato da usare in caso di fallimento.
     * @return true se l'esecuzione ha successo, altrimenti false.
     */
    auto executeQuery(sqlite3* db, const char* sql, const std::string& errorMsg) -> bool;

    /**
     * @brief Chiude la connessione al database, se aperta.
     */
    void closeDatabase(sqlite3* db);

private:

    std::string lastError;

}