# p2p_FileDistributor

## 2) Interpretacja i doprecyzowanie treści
- Plik będzie identyfikowany nazwą oraz hashem (MD5), jednak sam hash nie będzie wykorzystywany do innego celu niż identyfikacja pliku (mapowanie h >> n jest nie do końca intuicyjne, bo: adresy IP mogą być nieciągłe oraz mogą zostawiać dowolne, nieprzewidziane odległości między sobą w przypadku odłączania węzłów). Informacja o węźle przetrzymującym plik będzie zawarta w deskryptorze pliku.

## 3) Krótki opis funkcjonalny
- Węzęł dołączający się do sieci wysyła jedynie informację o swoim pojawieniu się poprzez Broadcast - pozostałe węzły sieci wyślą informacje o plikach, które u siebie przechowują poprzez bezpośrednie połączenie z nowym węzłem. Po tym dopiero następuje redestrybucja zasobów (przesłanie części własnych plików do nowego węzła).
- Węzeł dystrybuujący swój zasób oznacza go jako tymczasowo nieważny i rozsyła tę informację do wszystkich węzłów sieci. Po tym wykonuje transmisję oznaczonego zasobu do węzła przyjmującego pliki, który dopiero po prawidłowym odebraniu pliku - rozgłasza nowy deskryptor pliku (ze zmienionym węzłem przechowującym), już z flagą aktualności.
- Miejsce składowania nowego pliku jest wybierane przed jego pojawieniem się w sieci. Po poprawnej transmisji (jeśli taka była potrzebna - sytuacja, że węzeł przechowujący != właściciela) węzeł, który odebrał plik rozgłasza jego pojawienie się w sieci.
- Usunięcie pliku z sieci (unieważnienie przez właściciela) opiera się na wysłaniu bezpośredniego żądania usunięcia pliku do węzła, który ten plik aktualnie przechowuje. Oznacza on ten plik jako "usunięty" i rozsyła tę informację po całej sieci. Wtedy każdy z węzłów jest zobligowany do usunięcia deskryptora danego pliku ze swojej listy. Jeśli trwają jeszcze jakieś transmisje - to zostają one poprawnie zakończone po czym węzeł fizycznie usuwa zasób.
- Poprawne odłączenie się węzła opiera się na: rozgłoszeniu informacji w sieci o rozpoczęciu procedury odłączenia (zapobiegnie to sytuacji, kiedy w momencie "opróżniania" węzła powstaną nowe pliki, które zostałyby do niego przesłane), oznaczenia wszystkich przechowywanych zasobów jako "tymczasowo nieważnych" oraz ich redestrybucji do pozostałych węzłów sieci (które po odebraniu zasobów ponownie rozgłoszą ich aktualne deskryptory).
- Zaniknięcie węzła - w przypadku, jeśli węzeł zakończył pracę nieprawidłowo, to przy pierwszej próbie połączenia przez którykolwiek z węzłów informacja o tym zostanie rozgłoszona po sieci - wtedy każdy z węzłów usunie z własnej listy deskryptorów te wpisy, których zasoby które znajdowały się na usuniętym węźle. Wszystkie ewentualne transmisje zwrócą błędy, a jeśli będzie to możliwe - odwrócą jak najwięcej szkód (np. jeśli awaria nastąpiła przy redestrybucji danego zasobu - wtedy po prostu węzeł dystrybuujący odznaczy "tymczasową nieważność" przesyłanych plików).
- Konflikt nazw - nie występuje. Jeśli taka sytuacja nastąpi, to CLI wyświetli oba pliki wraz z fragmentem ich hasha.
- Konflikt hashy - przypadek, kiedy ten sam plik został zuploadowany przed propagacją informacji o poprzednio dodanym pliku. Wtedy każdy z węzłów automatycznie usuwa deskryptor pliku, który miał późniejszy "uploadTime".

## 4) Opis i analiza protokołów komunikacyjnych
### Opis komunikatów
Przesyłane wiadomości są opatrzone typem wiadomości (dołączenie do sieci, rozgłoszenie deskryptora, przesłanie pliku etc.) oraz rozmiarem danych, znajdujących się tuż za końcem struktury (mogą nimi być już deskryptory lub całe pliki). Typ danych znajdujących się za strukturą P2PMessage jest definiowany jednoznacznie przez typ wiadomości.
```c
struct P2PMessage {
	MessageType messageType;
	uint32_t additionalDataSize;
};
```

### Typy komunikatów
Komunikaty oznaczone jako UDP będą broadcastowane w sieci. Oznaczone jako TCP - będą przesyłane bezpośrednio do określonego węzła.
```c
enum class MessageType {
	// raportowanie stanu
	HELLO,				// UDP komunikat wysyłany przez nowoutworzony węzeł
	HELLO_REPLY,		// TCP odpowiedź od węzłów, które usłyszały HELLO. Dołącza tablicę deskryptorów plików, które znajdowały się w danej chwili w konkretnym 
	DISCONNECTING,		// UDP powiadomienie sieci o rozpoczęciu odłączania się
	CONNECTION_LOST,	// UDP powiadomienie sieci o utraceniu węzła o określonym IP (podanym w sekcji danych)
	CMD_REFUSED,		// TCP powiadomienie węzła, który złożył żądanie (np. o pobranie pliku) o braku możliwości wykonania transkacji (np. dostęp do pliku oznaczonego jako "tymczasowo nieważny" albo próba przesłania pliku do węzła w stanie "disconnecting")
	
	// zarządzanie plikami
	NEW_FILE,			// UDP powiadomienie sieci o nowym pliku o danym deskryptorze (podanym w sekcji danych)
	REVOKE_FILE,		// UDP powiadomienie sieci o usunięciu pliku o danym deskryptorze (podanym w sekcji danych)
	DISCARD_DESCRIPTOR,	// UDP oznacz podany deskryptor jako "tymczasowo nieważny"
	UPDATE_DESCRIPTOR,	// UDP rozgłoś nową wersję deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny")
	HOLDER_CHANGE,		// TCP przesłanie do określonego węzła zaktualizowanego deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny") oraz pliku
	FILE_TRANSFER,		// TCP przesłanie deskryptora oraz zawartości pliku do węzła, który wcześniej tego zażądał
	
	// interfejs użytkownika
	UPLOAD_FILE,		// TCP żądanie uploadu pliku, zawiera w sekcji danych: deskryptor oraz plik (jako tablica bajtów)
	GET_FILE,			// TCP żądanie przesłania pliku o danym deskryptorze (podanym w sekcji danych) od węzła przetrzymującego plik
	DELETE_FILE,		// TCP żądanie unieważnienia pliku o danym deskryptorze (podanym w sekcji danych) do węzła przetrzymującego plik
};
```

### Deskryptor pliku
Struktura, która jest dystrybuowana w sieci i definiuje pozycję i właściciela każdego pliku. Jej unikalnym polem jest hash MD5.
```c
struct FileDescriptor {
	MD5_T md5;
	char name[256];
	uint32_t size;
	clock_t uploadTime;
	uint32_t ownerIp;
	uint32_t holderIp;
};
```

### Zależności czasowe

![diagram sekwencji](https://github.com/saleph/p2p_FileDistributor/blob/master/sequencediagrams.png "Diagram sekwencji")

## 5) Moduły i realizacja współbieżności
### Moduły
1. Moduł węzła obsługujący protokół.
2. Moduł obsługi sieci (serwera TCP oraz gniazda UDP).
3. Moduł obsługi wielowątkowości (opakowanie pthreads).
3. CLI komunikujące się z API modułu węzła.
4. Moduł thread-safe loggera.

### Realizacja współbieżności
Proces początkowy realizuje obsługę interfejsu użytkownika oraz startuje osobny wątek do obsługi węzła sieci P2P. Wątek węzła uruchamia wątki odpowiedzialne za nasłuchiwanie broadcastu oraz obsługę serwera TCP. W tych wątkach znowuż po odebraniu wiadomości (w nasłuchu UDP) lub nawiązaniu połączenia (serwer TCP) zostaną powołane kolejne wątki, obsługujące już pojedyncze połączenie/pakiet. Przetwarzają je i aktualizują struktury opisujące węzeł.

![współbieżność](https://github.com/saleph/p2p_FileDistributor/blob/master/concurrencydiagram.png "Realizacja współbieżności")

## 6) Zarys koncepcji implementacji
Użyty zostanie C++ (jako pomoc przy budowie CLI oraz obsługi przesyłanych struktur w stopniu podstawowym - np. modyfikacje strumenia) wraz z częścią biblioteki `BOOST` - do przeprowadzania unit-testów oraz zbierania logów. Do obsługi współbieżności zostaną użyte POSIXowe `pthreads`, a do obsługi sieci - gniazda BSD.
