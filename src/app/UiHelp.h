#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <string_view>

namespace navalha::ui
{
enum class Language
{
    english = 0,
    portuguese,
    french,
    spanish
};

inline constexpr std::array<const char*, 4> languageCodes {
    "en", "pt", "fr", "es"};

inline int languageIndex(Language language) noexcept
{
    return static_cast<int>(language);
}

inline Language languageFromCode(const juce::String& code) noexcept
{
    if (code.equalsIgnoreCase("pt")) return Language::portuguese;
    if (code.equalsIgnoreCase("fr")) return Language::french;
    if (code.equalsIgnoreCase("es")) return Language::spanish;
    return Language::english;
}

inline juce::String languageCode(Language language)
{
    return languageCodes[static_cast<std::size_t>(languageIndex(language))];
}

struct LocalizedText
{
    const char* en;
    const char* pt;
    const char* fr;
    const char* es;
};

inline juce::String text(const LocalizedText& value, Language language)
{
    const std::array<const char*, 4> alternatives {
        value.en, value.pt, value.fr, value.es};
    return juce::String::fromUTF8(
        alternatives[static_cast<std::size_t>(languageIndex(language))]);
}

struct LearnEntry
{
    std::string_view key;
    LocalizedText title;
    LocalizedText body;
};

inline constexpr std::array learnEntries {
    LearnEntry {"audio", {"AUDIO ENGINE", "MOTOR DE ÁUDIO", "MOTEUR AUDIO", "MOTOR DE AUDIO"},
        {"Shows the real audio-device connection. Click to open audio settings.", "Mostra a conexão real do dispositivo. Clique para abrir as configurações de áudio.", "Affiche la connexion réelle du périphérique. Cliquez pour ouvrir les réglages audio.", "Muestra la conexión real del dispositivo. Pulse para abrir los ajustes de audio."}},
    LearnEntry {"openproject", {"OPEN PROJECT", "ABRIR PROJETO", "OUVRIR PROJET", "ABRIR PROYECTO"},
        {"Opens a Navalha project while leaving the referenced source recordings untouched.", "Abre um projeto Navalha sem alterar os arquivos de áudio referenciados.", "Ouvre un projet Navalha sans modifier les enregistrements source référencés.", "Abre un proyecto Navalha sin modificar los audios fuente referenciados."}},
    LearnEntry {"saveproject", {"SAVE PROJECT", "SALVAR PROJETO", "ENREGISTRER PROJET", "GUARDAR PROYECTO"},
        {"Saves the current state and source references; it does not render audio.", "Salva o estado e as referências das fontes; não renderiza áudio.", "Enregistre l’état et les références des sources, sans rendu audio.", "Guarda el estado y las referencias de las fuentes; no renderiza audio."}},
    LearnEntry {"saveportable", {"SAVE PORTABLE", "SALVAR PORTÁTIL", "PROJET PORTABLE", "GUARDAR PORTÁTIL"},
        {"Creates a self-contained Project v2 ZIP with copies of SOURCE A and B.", "Cria um ZIP Project v2 autônomo com cópias de SOURCE A e B.", "Crée un ZIP Project v2 autonome avec des copies de SOURCE A et B.", "Crea un ZIP Project v2 autónomo con copias de SOURCE A y B."}},
    LearnEntry {"play", {"PLAY", "PLAY", "PLAY", "PLAY"},
        {"Starts the current pattern and sequencer.", "Inicia o pattern e o sequenciador atuais.", "Démarre le pattern et le séquenceur courants.", "Inicia el patrón y el secuenciador actuales."}},
    LearnEntry {"stop", {"STOP", "STOP", "STOP", "STOP"},
        {"Stops playback and safely finalizes an active recording.", "Para a reprodução e finaliza com segurança uma gravação ativa.", "Arrête la lecture et finalise en sécurité un enregistrement actif.", "Detiene la reproducción y finaliza con seguridad una grabación activa."}},
    LearnEntry {"reset", {"RESET", "RESET", "RESET", "RESET"},
        {"Returns the sequencer cursor and transport clock to the first step.", "Retorna o cursor e o relógio do transporte ao primeiro passo.", "Ramène le curseur et l’horloge de transport au premier pas.", "Devuelve el cursor y el reloj de transporte al primer paso."}},
    LearnEntry {"rec", {"REC", "REC", "REC", "REC"},
        {"Starts or finalizes recording of the real stereo MASTER output. Recording stops automatically after five minutes.", "Inicia ou finaliza a gravação da saída MASTER estéreo real. A gravação para automaticamente após cinco minutos.", "Démarre ou finalise l’enregistrement de la sortie MASTER stéréo réelle. L’enregistrement s’arrête automatiquement après cinq minutes.", "Inicia o finaliza la grabación de la salida MASTER estéreo real. La grabación se detiene automáticamente después de cinco minutos."}},
    LearnEntry {"transportclock", {"TRANSPORT CLOCK", "RELÓGIO DO TRANSPORTE", "HORLOGE DE TRANSPORT", "RELOJ DE TRANSPORTE"},
        {"Shows elapsed playback or recording time.", "Mostra o tempo decorrido de reprodução ou gravação.", "Affiche le temps écoulé de lecture ou d’enregistrement.", "Muestra el tiempo transcurrido de reproducción o grabación."}},
    LearnEntry {"library", {"AUDIO LIBRARY", "BIBLIOTECA DE ÁUDIO", "BIBLIOTHÈQUE AUDIO", "BIBLIOTECA DE AUDIO"},
        {"Browse source material, then load or drag a WAV/AIFF to SOURCE A or B.", "Explore matérias sonoras e carregue ou arraste um WAV/AIFF para SOURCE A ou B.", "Parcourez les matériaux sonores puis chargez ou glissez un WAV/AIFF vers SOURCE A ou B.", "Explore materiales sonoros y cargue o arrastre un WAV/AIFF a SOURCE A o B."}},
    LearnEntry {"librarysearch", {"SEARCH FILES", "BUSCAR ARQUIVOS", "RECHERCHER DES FICHIERS", "BUSCAR ARCHIVOS"},
        {"Filters the visible library list by file name; it never changes or deletes a file.", "Filtra pelo nome a lista visível da biblioteca; não altera nem apaga nenhum arquivo.", "Filtre la liste visible par nom de fichier, sans modifier ni supprimer les fichiers.", "Filtra por nombre la lista visible; no modifica ni borra archivos."}},
    LearnEntry {"librarypreview", {"LIBRARY PREVIEW", "PRÉ-ESCUTA DA BIBLIOTECA", "PRÉ-ÉCOUTE DE LA BIBLIOTHÈQUE", "PREESCUCHA DE LA BIBLIOTECA"},
        {"Auditions the selected WAV/AIFF independently at a safe level. It does not load SOURCE A/B or alter the file.", "Permite ouvir o WAV/AIFF selecionado de forma independente e em nível seguro. Não carrega SOURCE A/B nem altera o arquivo.", "Écoute le WAV/AIFF sélectionné indépendamment à niveau sûr, sans charger SOURCE A/B ni modifier le fichier.", "Escucha el WAV/AIFF seleccionado de forma independiente y a nivel seguro, sin cargar SOURCE A/B ni modificar el archivo."}},
    LearnEntry {"activitylog", {"ACTIVITY LOG", "REGISTRO DE ATIVIDADE", "JOURNAL D’ACTIVITÉ", "REGISTRO DE ACTIVIDAD"},
        {"Keeps the four most recent messages. COPY exports the text; CLEAR empties only this view.", "Mantém as quatro mensagens mais recentes. COPY copia o texto; CLEAR limpa somente esta visualização.", "Conserve les quatre derniers messages. COPY copie le texte ; CLEAR vide uniquement cette vue.", "Conserva los cuatro mensajes más recientes. COPY copia el texto; CLEAR vacía solo esta vista."}},
    LearnEntry {"waveform", {"WAVEFORM", "FORMA DE ONDA", "FORME D’ONDE", "FORMA DE ONDA"},
        {"Shows the active source. Drag to select a region; BLADE clicks create cuts.", "Mostra a source ativa. Arraste para selecionar uma região; cliques em BLADE criam cortes.", "Affiche la source active. Glissez pour une région; les clics BLADE créent des coupes.", "Muestra la fuente activa. Arrastre para una región; los clics BLADE crean cortes."}},
    LearnEntry {"selectregion", {"SELECT REGION", "SELECIONAR REGIÃO", "SÉLECTIONNER RÉGION", "SELECCIONAR REGIÓN"},
        {"Dragging the waveform defines the larger region used to make slices.", "Arrastar na forma de onda define a região maior usada para criar slices.", "Glisser sur la forme d’onde définit la grande région utilisée pour les slices.", "Arrastrar en la forma de onda define la región mayor usada para crear slices."}},
    LearnEntry {"editslice", {"EDIT SLICE", "EDITAR SLICE", "ÉDITER SLICE", "EDITAR SLICE"},
        {"Edits the START and END boundaries of the selected slice.", "Edita os limites START e END do slice selecionado.", "Modifie les limites START et END du slice sélectionné.", "Edita los límites START y END del slice seleccionado."}},
    LearnEntry {"slice", {"SLICE CONTROLS", "CONTROLES DE SLICE", "CONTRÔLES DE SLICE", "CONTROLES DE SLICE"},
        {"Chooses SOURCE A/B and a slice, then adjusts START/END. SET commits the edit; PLAY SLICE auditions it.", "Escolhe SOURCE A/B e um slice, depois ajusta START/END. SET confirma a edição; PLAY SLICE permite ouvi-la.", "Choisit SOURCE A/B et un slice, puis règle START/END. SET valide ; PLAY SLICE permet l’écoute.", "Elige SOURCE A/B y un slice, luego ajusta START/END. SET confirma; PLAY SLICE permite escucharlo."}},
    LearnEntry {"blade", {"BLADE", "BLADE", "BLADE", "BLADE"},
        {"Each waveform click adds a non-destructive cut inside the region.", "Cada clique na forma de onda adiciona um corte não destrutivo na região.", "Chaque clic ajoute une coupe non destructive dans la région.", "Cada clic añade un corte no destructivo dentro de la región."}},
    LearnEntry {"divide", {"DIVIDE REGION", "DIVIDIR REGIÃO", "DIVISER RÉGION", "DIVIDIR REGIÓN"},
        {"Creates 4, 8, 16, 32 or 64 equal slices inside the selected region.", "Cria 4, 8, 16, 32 ou 64 slices iguais dentro da região selecionada.", "Crée 4, 8, 16, 32 ou 64 slices égaux dans la région sélectionnée.", "Crea 4, 8, 16, 32 o 64 slices iguales dentro de la región seleccionada."}},
    LearnEntry {"bpm", {"BPM", "BPM", "BPM", "BPM"},
        {"Sets sequencer tempo from 20 to 400 beats per minute.", "Define o andamento do sequenciador entre 20 e 400 batidas por minuto.", "Règle le tempo du séquenceur entre 20 et 400 battements par minute.", "Define el tempo del secuenciador entre 20 y 400 pulsos por minuto."}},
    LearnEntry {"pattern", {"PATTERN", "PATTERN", "PATTERN", "PATTERN"},
        {"Selects one of the eight step-pattern memories.", "Seleciona uma das oito memórias de pattern.", "Sélectionne une des huit mémoires de pattern.", "Selecciona una de las ocho memorias de patrón."}},
    LearnEntry {"order", {"PATTERN ORDER", "ORDEM DO PATTERN", "ORDRE DU PATTERN", "ORDEN DEL PATRÓN"},
        {"Builds or reshapes the eight-step order from SOURCE A, SOURCE B and GAP cells.", "Cria ou remodela a ordem de oito passos usando células SOURCE A, SOURCE B e GAP.", "Crée ou transforme l’ordre de huit pas avec des cellules SOURCE A, SOURCE B et GAP.", "Crea o transforma el orden de ocho pasos con celdas SOURCE A, SOURCE B y GAP."}},
    LearnEntry {"gesture", {"LIVE GESTURES", "GESTOS AO VIVO", "GESTES LIVE", "GESTOS EN VIVO"},
        {"Applies reversible structural gestures or short fragment actions to the current performance.", "Aplica gestos estruturais reversíveis ou ações curtas de fragmento à performance atual.", "Applique des gestes structurels réversibles ou de courtes actions de fragment à la performance.", "Aplica gestos estructurales reversibles o acciones breves de fragmento a la performance."}},
    LearnEntry {"timing", {"TIMING", "TEMPORIZAÇÃO", "TEMPORISATION", "TEMPORIZACIÓN"},
        {"GRID is regular, FREE varies durations, and JITTER moves intervals around the grid.", "GRID é regular, FREE varia as durações e JITTER desloca os intervalos ao redor da grade.", "GRID est régulier, FREE varie les durées et JITTER déplace les intervalles autour de la grille.", "GRID es regular, FREE varía duraciones y JITTER desplaza intervalos alrededor de la rejilla."}},
    LearnEntry {"pitch", {"HERITAGE PITCH", "HERITAGE PITCH", "HERITAGE PITCH", "HERITAGE PITCH"},
        {"Uses the historical 24-position range from −12 through +11 semitones.", "Usa a faixa histórica de 24 posições, de −12 a +11 semitons.", "Utilise la plage historique de 24 positions, de −12 à +11 demi-tons.", "Usa el rango histórico de 24 posiciones, de −12 a +11 semitonos."}},
    LearnEntry {"assisted", {"ASSISTED PERFORMER", "PERFORMER ASSISTIDO", "PERFORMER ASSISTÉ", "PERFORMER ASISTIDO"},
        {"Makes bounded musical decisions but never controls PLAY, STOP or REC.", "Toma decisões musicais limitadas, mas nunca controla PLAY, STOP ou REC.", "Prend des décisions musicales limitées sans contrôler PLAY, STOP ni REC.", "Toma decisiones musicales limitadas, pero nunca controla PLAY, STOP ni REC."}},
    LearnEntry {"form", {"FORM", "FORMA", "FORME", "FORMA"},
        {"Directs the piece through INTRO, DEVELOP, RUPTURE, CLIMAX and EXIT scenes.", "Conduz a peça pelas cenas INTRO, DEVELOP, RUPTURE, CLIMAX e EXIT.", "Conduit la pièce à travers INTRO, DEVELOP, RUPTURE, CLIMAX et EXIT.", "Conduce la pieza por INTRO, DEVELOP, RUPTURE, CLIMAX y EXIT."}},
    LearnEntry {"trace", {"TRACE XY", "TRAÇO XY", "TRACE XY", "TRAZA XY"},
        {"Records and optionally loops control movement; it does not record audio.", "Grava e opcionalmente repete movimentos de controle; não grava áudio.", "Enregistre et boucle éventuellement le mouvement de contrôle, pas l’audio.", "Graba y opcionalmente repite movimientos de control; no graba audio."}},
    LearnEntry {"motif", {"MOTIF MEMORY", "MEMÓRIA DE MOTIVOS", "MÉMOIRE DE MOTIFS", "MEMORIA DE MOTIVOS"},
        {"Stores, recalls and varies reusable musical snapshots with independent locks.", "Armazena, recupera e varia snapshots musicais reutilizáveis com locks independentes.", "Stocke, rappelle et varie des instantanés musicaux avec verrous indépendants.", "Guarda, recupera y varía instantáneas musicales con bloqueos independientes."}},
    LearnEntry {"mixer", {"SOURCE MIXER", "MIXER DE FONTES", "MIXEUR DE SOURCES", "MEZCLADOR DE FUENTES"},
        {"Balances SOURCE A/B before pitch and sends their stereo sum to MASTER OUTPUT.", "Equilibra SOURCE A/B antes do pitch e envia a soma estéreo ao MASTER OUTPUT.", "Équilibre SOURCE A/B avant le pitch et envoie leur somme stéréo vers MASTER OUTPUT.", "Equilibra SOURCE A/B antes del pitch y envía su suma estéreo a MASTER OUTPUT."}},
    LearnEntry {"mixerlevel", {"SOURCE LEVEL", "NÍVEL DA FONTE", "NIVEAU DE LA SOURCE", "NIVEL DE LA FUENTE"},
        {"Sets source gain before the stereo sum: 0 is silent, 1.00 is unity and values above 1 add gain.", "Define o ganho antes da soma estéreo: 0 silencia, 1,00 mantém o nível e valores acima de 1 acrescentam ganho.", "Règle le gain avant la somme stéréo : 0 coupe, 1,00 est unitaire et une valeur supérieure ajoute du gain.", "Ajusta la ganancia antes de la suma estéreo: 0 silencia, 1,00 mantiene el nivel y valores mayores añaden ganancia."}},
    LearnEntry {"mixerpan", {"SOURCE PAN", "PAN DA FONTE", "PANORAMIQUE DE LA SOURCE", "PAN DE LA FUENTE"},
        {"Places one source in the stereo field: −1 is left, 0 is centre and +1 is right.", "Posiciona uma fonte no campo estéreo: −1 é esquerda, 0 é centro e +1 é direita.", "Place une source dans le champ stéréo : −1 à gauche, 0 au centre et +1 à droite.", "Sitúa una fuente en el campo estéreo: −1 izquierda, 0 centro y +1 derecha."}},
    LearnEntry {"mixerwidth", {"SOURCE WIDTH", "LARGURA DA FONTE", "LARGEUR DE LA SOURCE", "ANCHURA DE LA FUENTE"},
        {"Controls stereo width: 0 folds to mono, 1 preserves the source and 2 exaggerates side information.", "Controla a largura estéreo: 0 reduz a mono, 1 preserva a fonte e 2 amplia a informação lateral.", "Contrôle la largeur stéréo : 0 replie en mono, 1 préserve la source et 2 accentue les côtés.", "Controla la anchura estéreo: 0 reduce a mono, 1 conserva la fuente y 2 exagera la información lateral."}},
    LearnEntry {"mixermute", {"SOURCE MUTE", "MUTE DA FONTE", "SILENCE DE LA SOURCE", "MUTE DE LA FUENTE"},
        {"Silences only this source without changing its level setting.", "Silencia somente esta fonte sem alterar seu ajuste de nível.", "Coupe uniquement cette source sans modifier son réglage de niveau.", "Silencia solo esta fuente sin cambiar su ajuste de nivel."}},
    LearnEntry {"mixersolo", {"SOURCE SOLO", "SOLO DA FONTE", "SOLO DE LA SOURCE", "SOLO DE LA FUENTE"},
        {"Auditions this source without the other one. If both are soloed, both remain audible.", "Permite ouvir esta fonte sem a outra. Se ambas estiverem em SOLO, as duas permanecem audíveis.", "Écoute cette source sans l’autre. Si les deux sont en SOLO, les deux restent audibles.", "Permite escuchar esta fuente sin la otra. Si ambas están en SOLO, las dos permanecen audibles."}},
    LearnEntry {"mixerbalance", {"A/B BALANCE", "BALANÇO A/B", "BALANCE A/B", "BALANCE A/B"},
        {"Cross-balances the mixed sources: −1 favours A, 0 keeps both and +1 favours B.", "Faz o balanço entre as fontes: −1 favorece A, 0 mantém ambas e +1 favorece B.", "Dose les sources : −1 favorise A, 0 garde les deux et +1 favorise B.", "Balancea las fuentes: −1 favorece A, 0 mantiene ambas y +1 favorece B."}},
    LearnEntry {"output", {"MASTER / TECHNICAL OUTPUT", "MASTER / SAÍDA TÉCNICA", "MASTER / SORTIE TECHNIQUE", "MASTER / SALIDA TÉCNICA"},
        {"MASTER shapes the instrument creatively. OUTPUT TRIM only attenuates −24 to 0 dB before the true-peak limiter; MUTE uses a click-free ramp. Device reconnect resumes from silence.", "MASTER molda criativamente o instrumento. OUTPUT TRIM apenas atenua de −24 a 0 dB antes do limiter true peak; MUTE usa rampa sem clique. A reconexão do dispositivo retoma a partir do silêncio.", "MASTER façonne l’instrument de manière créative. OUTPUT TRIM atténue seulement de −24 à 0 dB avant le limiteur true peak ; MUTE utilise une rampe sans clic. La reconnexion reprend depuis le silence.", "MASTER moldea creativamente el instrumento. OUTPUT TRIM solo atenúa de −24 a 0 dB antes del limitador true peak; MUTE usa una rampa sin clic. La reconexión reanuda desde silencio."}},
    LearnEntry {"recordformat", {"RECORD FORMAT", "FORMATO DE GRAVAÇÃO", "FORMAT D’ENREGISTREMENT", "FORMATO DE GRABACIÓN"},
        {"Chooses PCM16, PCM24 or FLOAT32 for the next stereo MASTER recording.", "Escolhe PCM16, PCM24 ou FLOAT32 para a próxima gravação MASTER estéreo.", "Choisit PCM16, PCM24 ou FLOAT32 pour le prochain enregistrement MASTER stéréo.", "Elige PCM16, PCM24 o FLOAT32 para la próxima grabación MASTER estéreo."}},
    LearnEntry {"voices", {"VIRTUAL VOICES", "VOZES VIRTUAIS", "VOIX VIRTUELLES", "VOCES VIRTUALES"},
        {"Adds two parallel internal voices that reuse SOURCE A/B with safe headroom.", "Adiciona duas vozes internas paralelas que reutilizam SOURCE A/B com margem segura.", "Ajoute deux voix internes parallèles réutilisant SOURCE A/B avec une marge sûre.", "Añade dos voces internas paralelas que reutilizan SOURCE A/B con margen seguro."}},
    LearnEntry {"takes", {"TAKE TIMELINE", "LINHA DO TEMPO DE TAKES", "CHRONOLOGIE DES PRISES", "LÍNEA DE TIEMPO DE TOMAS"},
        {"Opens recording history, review metadata and TAKE-to-SOURCE actions.", "Abre o histórico de gravações, metadados de revisão e ações TAKE para SOURCE.", "Ouvre l’historique, les métadonnées de revue et les actions PRISE vers SOURCE.", "Abre el historial, metadatos de revisión y acciones TOMA a SOURCE."}},
    LearnEntry {"dual", {"DUAL MONITOR", "DOIS MONITORES", "DOUBLE ÉCRAN", "DOBLE MONITOR"},
        {"Moves live performance to the second display while the main display keeps editing and production controls.", "Move a performance ao vivo para a segunda tela, mantendo edição e produção na tela principal.", "Place la performance sur le second écran et garde édition et production sur l’écran principal.", "Mueve la performance al segundo monitor y mantiene edición y producción en el principal."}},
    LearnEntry {"masterwindow", {"MASTER WINDOW", "JANELA MASTER", "FENÊTRE MASTER", "VENTANA MASTER"},
        {"Opens the separate offline mastering and album-rendering workspace.", "Abre o workspace separado de masterização offline e renderização de álbum.", "Ouvre l’espace séparé de mastering hors ligne et de rendu d’album.", "Abre el espacio separado de masterización offline y renderizado de álbum."}},
    LearnEntry {"production", {"TAKES / MASTER", "TAKES / MASTER", "PRISES / MASTER", "TOMAS / MASTER"},
        {"Opens the linked production workspace: review takes on the left and prepare track or album masters on the right.", "Abre o workspace de produção integrado: revise os takes à esquerda e prepare masters de faixa ou álbum à direita.", "Ouvre l’espace de production intégré : révisez les prises à gauche et préparez les masters piste ou album à droite.", "Abre el espacio de producción integrado: revise las tomas a la izquierda y prepare masters de pista o álbum a la derecha."}},
    LearnEntry {"language", {"LANGUAGE", "IDIOMA", "LANGUE", "IDIOMA"},
        {"Changes contextual help and tutorial text between English, Portuguese, French and Spanish.", "Alterna os textos da ajuda contextual e do tutorial entre inglês, português, francês e espanhol.", "Change les textes de l’aide contextuelle et du tutoriel entre anglais, portugais, français et espagnol.", "Cambia los textos de ayuda contextual y tutorial entre inglés, portugués, francés y español."}},
    LearnEntry {"tutorial", {"TUTORIAL", "TUTORIAL", "TUTORIEL", "TUTORIAL"},
        {"Opens the ten-chapter guide inherited from the PD v0.28.1 workflow.", "Abre o guia de dez capítulos herdado do fluxo de trabalho da versão PD v0.28.1.", "Ouvre le guide en dix chapitres hérité du flux de travail PD v0.28.1.", "Abre la guía de diez capítulos heredada del flujo de trabajo PD v0.28.1."}},
    LearnEntry {"app", {"ABOUT", "SOBRE", "À PROPOS", "ACERCA DE"},
        {"Shows version, reference build, authorship and license information.", "Mostra versão, build de referência, autoria e licença.", "Affiche version, build de référence, auteurs et licence.", "Muestra versión, build de referencia, autoría y licencia."}},
    LearnEntry {"learn", {"LEARN", "LEARN", "LEARN", "LEARN"},
        {"When active, moving over or focusing a control explains it in the fixed Activity Log panel.", "Quando ativo, passar ou focar um controle mostra sua explicação no painel fixo Activity Log.", "Lorsqu’il est actif, survoler ou focaliser un contrôle affiche son explication dans le panneau fixe Activity Log.", "Cuando está activo, pasar o enfocar un control muestra su explicación en el panel fijo Activity Log."}},
    LearnEntry {"view", {"VIEW", "VISUALIZAÇÃO", "AFFICHAGE", "VISTA"},
        {"Chooses automatic sizing or a larger interface scale. Small displays keep scrolling available.", "Escolhe tamanho automático ou uma escala maior. Telas pequenas mantêm a rolagem disponível.", "Choisit le dimensionnement automatique ou une échelle agrandie. Le défilement reste disponible sur les petits écrans.", "Elige tamaño automático o una escala mayor. Las pantallas pequeñas mantienen el desplazamiento."}}
};

inline const LearnEntry* findLearnEntry(std::string_view key) noexcept
{
    for (const auto& entry : learnEntries)
        if (entry.key == key)
            return &entry;
    return nullptr;
}

struct TutorialChapter
{
    LocalizedText title;
    LocalizedText body;
};

inline constexpr std::array tutorialChapters {
    TutorialChapter {{"1 · START", "1 · INÍCIO", "1 · DÉMARRAGE", "1 · INICIO"},
        {"Load or drag a WAV/AIFF to SOURCE A or B. Choose a region, divide it into slices, then press PLAY. The original recording is never edited.", "Carregue ou arraste um WAV/AIFF para SOURCE A ou B. Escolha uma região, divida em slices e pressione PLAY. A gravação original nunca é alterada.", "Chargez ou glissez un WAV/AIFF vers SOURCE A ou B. Choisissez une région, divisez-la en slices puis appuyez sur PLAY. L’original n’est jamais modifié.", "Cargue o arrastre un WAV/AIFF a SOURCE A o B. Elija una región, divídala en slices y pulse PLAY. El original nunca se modifica."}},
    TutorialChapter {{"2 · LIBRARY", "2 · BIBLIOTECA", "2 · BIBLIOTHÈQUE", "2 · BIBLIOTECA"},
        {"Use AUDIO LIBRARY to browse source material. LOAD A/B or drag a file directly to the desired waveform lane.", "Use AUDIO LIBRARY para explorar matérias sonoras. Use LOAD A/B ou arraste o arquivo para a faixa desejada.", "Utilisez AUDIO LIBRARY pour parcourir les sources. LOAD A/B ou glissez le fichier vers la piste voulue.", "Use AUDIO LIBRARY para explorar materiales. Use LOAD A/B o arrastre el archivo a la pista deseada."}},
    TutorialChapter {{"3 · REGION", "3 · REGIÃO", "3 · RÉGION", "3 · REGIÓN"},
        {"SELECT REGION defines the working area. WHOLE restores the complete source. PLAY SLICE auditions without modifying the file.", "SELECT REGION define a área de trabalho. WHOLE restaura a source completa. PLAY SLICE permite ouvir sem modificar o arquivo.", "SELECT REGION définit la zone de travail. WHOLE restaure toute la source. PLAY SLICE écoute sans modifier le fichier.", "SELECT REGION define el área de trabajo. WHOLE restaura toda la fuente. PLAY SLICE permite escuchar sin modificar el archivo."}},
    TutorialChapter {{"4 · SLICES", "4 · SLICES", "4 · SLICES", "4 · SLICES"},
        {"Divide the region into 4–64 slices, refine START/END in EDIT SLICE, or add non-destructive cuts with BLADE.", "Divida a região em 4–64 slices, refine START/END em EDIT SLICE ou adicione cortes não destrutivos com BLADE.", "Divisez la région en 4–64 slices, affinez START/END dans EDIT SLICE ou ajoutez des coupes BLADE non destructives.", "Divida la región en 4–64 slices, ajuste START/END en EDIT SLICE o añada cortes BLADE no destructivos."}},
    TutorialChapter {{"5 · PATTERN", "5 · PATTERN", "5 · PATTERN", "5 · PATRÓN"},
        {"The eight cells define slice order. RANDOM, INTERLEAVE, FORWARD, REVERSE, ZERO and GAP create or reshape patterns.", "As oito células definem a ordem dos slices. RANDOM, INTERLEAVE, FORWARD, REVERSE, ZERO e GAP criam ou remodelam patterns.", "Les huit cellules définissent l’ordre des slices. RANDOM, INTERLEAVE, FORWARD, REVERSE, ZERO et GAP créent ou transforment les patterns.", "Las ocho celdas definen el orden de slices. RANDOM, INTERLEAVE, FORWARD, REVERSE, ZERO y GAP crean o transforman patrones."}},
    TutorialChapter {{"6 · SOUND", "6 · SOM", "6 · SON", "6 · SONIDO"},
        {"TIME sets BPM and timing behavior. HERITAGE PITCH provides the original 24 positions. SOURCE MIXER controls A/B level, pan, width and balance before MASTER.", "TIME define BPM e comportamento temporal. HERITAGE PITCH oferece as 24 posições originais. SOURCE MIXER controla nível, pan, width e balance antes do MASTER.", "TIME règle BPM et comportement temporel. HERITAGE PITCH offre les 24 positions originales. SOURCE MIXER contrôle niveau, pan, width et balance avant MASTER.", "TIME define BPM y comportamiento temporal. HERITAGE PITCH ofrece las 24 posiciones originales. SOURCE MIXER controla nivel, pan, width y balance antes de MASTER."}},
    TutorialChapter {{"7 · AUTO PERFORMER", "7 · PERFORMER AUTOMÁTICO", "7 · PERFORMER AUTOMATIQUE", "7 · PERFORMER AUTOMÁTICO"},
        {"ASSISTED PERFORMER makes bounded decisions from enabled vocabularies. Locks protect choices. PLAY, STOP and REC always remain human.", "ASSISTED PERFORMER toma decisões limitadas a partir dos vocabulários ativos. Locks protegem escolhas. PLAY, STOP e REC permanecem humanos.", "ASSISTED PERFORMER prend des décisions limitées selon les vocabulaires actifs. Les verrous protègent les choix. PLAY, STOP et REC restent humains.", "ASSISTED PERFORMER toma decisiones limitadas según los vocabularios activos. Los bloqueos protegen elecciones. PLAY, STOP y REC siguen siendo humanos."}},
    TutorialChapter {{"8 · PROJECTS", "8 · PROJETOS", "8 · PROJETS", "8 · PROYECTOS"},
        {"SAVE PROJECT stores state and source references. SAVE PORTABLE creates a Project v2 ZIP with copied sources for transport to another computer.", "SAVE PROJECT guarda estado e referências. SAVE PORTABLE cria um ZIP Project v2 com cópias das fontes para levar a outro computador.", "SAVE PROJECT garde l’état et les références. SAVE PORTABLE crée un ZIP Project v2 avec les sources copiées pour un autre ordinateur.", "SAVE PROJECT guarda estado y referencias. SAVE PORTABLE crea un ZIP Project v2 con fuentes copiadas para otro ordenador."}},
    TutorialChapter {{"9 · REC / RESAMPLE", "9 · REC / REAMOSTRAGEM", "9 · REC / RÉÉCHANTILLONNAGE", "9 · REC / REMUESTREO"},
        {"REC captures the real stereo MASTER. Finalized recordings enter TAKE TIMELINE, where metadata can be reviewed and a take can return as SOURCE A or B.", "REC captura o MASTER estéreo real. Gravações finalizadas entram em TAKE TIMELINE, onde metadados podem ser revistos e um take pode voltar como SOURCE A ou B.", "REC capture le MASTER stéréo réel. Les prises finalisées entrent dans TAKE TIMELINE, où leurs métadonnées sont révisées et une prise peut revenir comme SOURCE A ou B.", "REC captura el MASTER estéreo real. Las tomas finalizadas entran en TAKE TIMELINE, donde se revisan metadatos y una toma puede volver como SOURCE A o B."}},
    TutorialChapter {{"10 · HELP", "10 · AJUDA", "10 · AIDE", "10 · AYUDA"},
        {"Activate LEARN and move the pointer over a control. Its explanation appears in the fixed Activity Log panel without covering the instrument.", "Ative LEARN e passe o ponteiro sobre um controle. A explicação aparece no painel fixo Activity Log sem cobrir o instrumento.", "Activez LEARN et passez le pointeur sur un contrôle. Son explication apparaît dans le panneau fixe Activity Log sans couvrir l’instrument.", "Active LEARN y pase el puntero sobre un control. Su explicación aparece en el panel fijo Activity Log sin cubrir el instrumento."}}
};

static_assert(tutorialChapters.size() == 10);
}
