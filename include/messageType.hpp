#ifndef INCLUDE_MESSAGETYPE_HPP_
#define INCLUDE_MESSAGETYPE_HPP_

#include <boost/functional/hash.hpp>

/// Enum class describing whole protocol abilities.
enum class MessageType {
	// raportowanie stanu
	HELLO,				//< UDP komunikat wysyłany przez nowoutworzony węzeł
	HELLO_REPLY,		//< TCP odpowiedź od węzłów, które usłyszały HELLO. Dołącza tablicę deskryptorów plików, które znajdowały się w danej chwili w konkretnym
	DISCONNECTING,		//< UDP powiadomienie sieci o rozpoczęciu odłączania się
	CONNECTION_LOST,	//< UDP powiadomienie sieci o utraceniu węzła o określonym IP (podanym w sekcji danych)
	CMD_REFUSED,		//< TCP powiadomienie węzła, który złożył żądanie (np. o pobranie pliku) o braku możliwości wykonania transkacji (np. dostęp do pliku oznaczonego jako "tymczasowo nieważny" albo próba przesłania pliku do węzła w stanie "disconnecting")
    SHUTDOWN,           //< UDP ostatnia wiadomosc wyslana przez zamykajacy sie wezel - ostatecznie zabija wszystkie deskryptory, ktore nadal nie zostaly zupdatetowane

	// zarządzanie plikami
	NEW_FILE,			//< UDP powiadomienie sieci o nowym pliku o danym deskryptorze (podanym w sekcji danych)
	REVOKE_FILE,		//< UDP powiadomienie sieci o usunięciu pliku o danym deskryptorze (podanym w sekcji danych)
	DISCARD_DESCRIPTOR,	//< UDP oznacz podany deskryptor jako "tymczasowo nieważny"
	UPDATE_DESCRIPTOR,	//< UDP rozgłoś nową wersję deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny")
	HOLDER_CHANGE,		//< TCP przesłanie do określonego węzła zaktualizowanego deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny") oraz pliku
	FILE_TRANSFER,		//< TCP przesłanie deskryptora oraz zawartości pliku do węzła, który wcześniej tego zażądał

	// interfejs użytkownika
	UPLOAD_FILE,		//< TCP żądanie uploadu pliku, zawiera w sekcji danych: deskryptor oraz plik (jako tablica bajtów)
	GET_FILE,			//< TCP żądanie przesłania pliku o danym deskryptorze (podanym w sekcji danych) od węzła przetrzymującego plik
	DELETE_FILE,		//< TCP żądanie unieważnienia pliku o danym deskryptorze (podanym w sekcji danych) do węzła przetrzymującego plik
};


namespace std {
    template <>
    struct hash<MessageType>
    {
        std::size_t operator()(const MessageType& messageType) const {
            auto resourceId = static_cast<typename std::underlying_type<MessageType>::type>(messageType);
            std::size_t hashValue = 0;
            boost::hash_combine(hashValue, resourceId);
            return hashValue;
        }
    };
}



#endif /* INCLUDE_MESSAGETYPE_HPP_ */
