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

## 4) Opis i analiza protokołów komunikacyjnych
### Opis komunikatów
Przesyłane wiadomości są opatrzone typem wiadomości (dołączenie do sieci, rozgłoszenie deskryptora, przesłanie pliku etc.) oraz rozmiarem danych, znajdujących się tuż za końcem struktury (mogą nimi być już deskryptory lub całe pliki). Typ danych znajdujących się za strukturą P2PMessage jest definiowany jednoznacznie przez typ wiadomości.
```c
struct P2PMessage {
	MessageType messageType;
	uint32_t additionalDataSize;
}
```

### Deskryptor pliku
Struktura, która jest dystrybuowana w sieci i definiuje pozycję i właściciela każdego pliku. 
```c
struct FileDescriptor {
	char name[256];
	MD5_T md5;
	uint32_t ownerIp;
	uint32_t holderIp;
	uint32_t flags;
}
```
