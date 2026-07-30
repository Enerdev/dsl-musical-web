#ifndef ANALIZADOR_LEXICO_H
#define ANALIZADOR_LEXICO_H

#include <iostream>
#include <fstream>
#include <string.h>
#include <list>
#include <iomanip>
#include <cctype>

using namespace std;

// ============================================================
//  DEFINICION DE TOKENS (estilo #define)
// ============================================================

// Palabras clave - Estructura
#define HOJA            0
#define A4              1
#define TITULO          2
#define AUTOR           3
#define PENTAGRAMA      4
#define CLAVE           5
#define COMPAS          6
#define BIS             7
#define TEMPO           8
#define VOLUMEN         9
#define REPETIR         10
#define FIN_REPETIR     11

// Palabras clave - Sentencias de programacion
#define FIN_KW          12   // FIN (cierre de SI / REPETIR)
#define DECLARAR        13
#define MOSTRAR         14
#define INGRESAR        15

// Notas musicales
#define DO              20
#define RE              21
#define MI              22
#define FA              23
#define SOL             24
#define LA              25
#define SI              26
#define SILENCIO        27

// Duraciones
#define NEGRA           30
#define BLANCA          31
#define REDONDA         32
#define CORCHEA         33
#define SEMICORCHEA     34
#define FUSA            35

// Instrumentos
#define INSTRUMENTO     40
#define PIANO           41
#define GUITARRA        42
#define VIOLIN          43
#define FLAUTA          44
#define TROMPETA        45
#define VIOLONCHELO     46
#define CELLO           47
#define OBOE            48

// Modos armonicos
#define MAYOR           50
#define MENOR           51

// Signos
#define DOSPUNTOS       60
#define BARRA           61
#define ALLAVE          62
#define CLLAVE          63
#define APARENTESIS     64
#define CPARENTESIS     65
#define SOSTENIDO       66
#define BEMOL           67

// Literales
#define CADENA          70
#define ENTERO          71
#define FRACCION        72

// Identificadores de usuario y tipos de dato
#define IDENTIFICADOR   80
#define TIPO_ENTERO     82
#define TIPO_CADENA     83
#define TIPO_BOOLEANO   84
#define VERDADERO       85
#define FALSO           86
#define IGUAL           87   // =

// Especiales
#define FIN             666
#define ERROR           999

// Errores lexicos tipificados
#define EL1             901   // Cadena sin cerrar
#define EL2             902   // Fraccion mal formada
#define EL3             903   // Identificador desconocido
#define EL4             904   // Caracter ilegal

// Para analisis semantico (compatibilidad futura)
#define null   "NULL"
#define vacio  "-"

// ============================================================
//  CLASE: Atributos
// ============================================================
class Atributos {
    public:
        string lexema;
        int    token;
        string tipo;
        string valor;
        string estado;

        Atributos() {
            lexema = "";
            token  = -999;
            tipo   = "";
            valor  = null;
            estado = "";
        }
        Atributos(string lex, int tok, string tip, string val, string est) {
            lexema = lex;
            token  = tok;
            tipo   = tip;
            valor  = val;
            estado = est;
        }
        void Mostrar() {
            cout << left
                 << "Tipo("   << setw(12) << tipo   << ") "
                 << "Lexema(" << setw(16) << lexema << ") "
                 << "Token("  << setw(4)  << token  << ") "
                 << "Valor("  << setw(8)  << valor  << ") "
                 << "Estado(" << estado   << ")"
                 << endl;
        }
};

// ============================================================
//  CLASE: TablaSimbolos
// ============================================================
class TablaSimbolos {
    private:
        list<Atributos> tabla;
    public:
        void Insertar(string lex, int tok, string tip, string val, string est) {
            Atributos attr(lex, tok, tip, val, est);
            tabla.push_back(attr);
        }
        bool ActualizarValor(string lex, string val) {
            for (auto &item : tabla) {
                if (item.lexema == lex) { item.valor = val; return true; }
            }
            return false;
        }
        bool ActualizarTipo(string lex, string tipo) {
            for (auto &item : tabla) {
                if (item.lexema == lex) { item.tipo = tipo; return true; }
            }
            return false;
        }
        bool ActualizarEstado(string lex, string est) {
            for (auto &item : tabla) {
                if (item.lexema == lex) { item.estado = est; return true; }
            }
            return false;
        }
        bool Buscar(string lex, Atributos &attr) {
            for (auto item : tabla) {
                if (item.lexema == lex) { attr = item; return true; }
            }
            return false;
        }
        bool BuscarPClave(string lex, Atributos &attr) {
            for (auto item : tabla) {
                if (item.lexema == lex && item.tipo == "pclave") {
                    attr = item; return true;
                }
            }
            return false;
        }
        void Mostrar() {
            for (auto item : tabla) item.Mostrar();
        }
        list<Atributos> getTabla() { return tabla; }
};

// ============================================================
//  CLASE: Analisis Lexico
// ============================================================
class AnalizadorLexico {
    private:
        // -- FIX seguridad: buffer ampliado + tamano expuesto como constante --
        // Antes: char cad[5000] + strcpy(cad, input) sin verificar limite.
        // Con entrada local (archivos .mus cortos) nunca se notaba, pero
        // con entrada web (texto pegado por el usuario) esto era un
        // desborde de buffer real. Ahora se amplia el limite y se corta
        // la copia de forma segura en el constructor.
        static const int MAX_CAD = 20000;

        int  i;              // puntero al caracter actual
        char cad[MAX_CAD];   // cadena de entrada completa

        string numero;     // ultimo numero leido
        string variable;   // ultimo identificador leido
        string cadenaStr;  // contenido de ultima cadena "..."
        string fraccion;   // ultima fraccion leida (ej: 3/4)

        int nLinea;        // linea actual (para mensajes de error)

        TablaSimbolos ts;  // tabla de simbolos

        // ── Contadores para resumen final ──
        int contTokens;
        int contErrores;

    public:
        AnalizadorLexico(const char input[]) {
            // -- FIX: copia acotada al tamano real de cad, nunca desborda --
            size_t len = strlen(input);
            if (len >= (size_t)MAX_CAD) {
                len = MAX_CAD - 1;
                cerr << "  [AVISO] El codigo fuente excede " << MAX_CAD
                     << " caracteres y fue truncado.\n";
            }
            memcpy(cad, input, len);
            cad[len] = '\0';

            i          = 0;
            nLinea     = 1;
            contTokens = 0;
            contErrores= 0;

            // ── Palabras clave: Estructura ──
            ts.Insertar("HOJA",        HOJA,        "pclave", vacio, vacio);
            ts.Insertar("A4",          A4,          "pclave", vacio, vacio);
            ts.Insertar("TITULO",      TITULO,      "pclave", vacio, vacio);
            ts.Insertar("AUTOR",       AUTOR,       "pclave", vacio, vacio);
            ts.Insertar("PENTAGRAMA",  PENTAGRAMA,  "pclave", vacio, vacio);
            ts.Insertar("CLAVE",       CLAVE,       "pclave", vacio, vacio);
            ts.Insertar("COMPAS",      COMPAS,      "pclave", vacio, vacio);
            ts.Insertar("BIS",         BIS,         "pclave", vacio, vacio);
            ts.Insertar("TEMPO",       TEMPO,       "pclave", vacio, vacio);
            ts.Insertar("VOLUMEN",     VOLUMEN,     "pclave", vacio, vacio);
            ts.Insertar("REPETIR",     REPETIR,     "pclave", vacio, vacio);
            ts.Insertar("FIN_REPETIR", FIN_REPETIR, "pclave", vacio, vacio);

            // ── Notas ──
            ts.Insertar("DO",       DO,       "nota", vacio, vacio);
            ts.Insertar("RE",       RE,       "nota", vacio, vacio);
            ts.Insertar("MI",       MI,       "nota", vacio, vacio);
            ts.Insertar("FA",       FA,       "nota", vacio, vacio);
            ts.Insertar("SOL",      SOL,      "nota", vacio, vacio);
            ts.Insertar("LA",       LA,       "nota", vacio, vacio);
            ts.Insertar("SI",       SI,       "nota", vacio, vacio);
            ts.Insertar("SILENCIO", SILENCIO, "nota", vacio, vacio);

            // ── Duraciones ──
            ts.Insertar("NEGRA",       NEGRA,       "duracion", vacio, vacio);
            ts.Insertar("BLANCA",      BLANCA,      "duracion", vacio, vacio);
            ts.Insertar("REDONDA",     REDONDA,     "duracion", vacio, vacio);
            ts.Insertar("CORCHEA",     CORCHEA,     "duracion", vacio, vacio);
            ts.Insertar("SEMICORCHEA", SEMICORCHEA, "duracion", vacio, vacio);
            ts.Insertar("FUSA",        FUSA,        "duracion", vacio, vacio);

            // ── Instrumentos ──
            ts.Insertar("INSTRUMENTO", INSTRUMENTO, "pclave",      vacio, vacio);
            ts.Insertar("PIANO",       PIANO,       "instrumento", vacio, vacio);
            ts.Insertar("GUITARRA",    GUITARRA,    "instrumento", vacio, vacio);
            ts.Insertar("VIOLIN",      VIOLIN,      "instrumento", vacio, vacio);
            ts.Insertar("FLAUTA",      FLAUTA,      "instrumento", vacio, vacio);
            ts.Insertar("TROMPETA",    TROMPETA,    "instrumento", vacio, vacio);

            // ── Modos armonicos ──
            ts.Insertar("MAYOR", MAYOR, "modo", vacio, vacio);
            ts.Insertar("MENOR", MENOR, "modo", vacio, vacio);

            // ── Instrumentos adicionales ──
            ts.Insertar("VIOLONCHELO", VIOLONCHELO, "instrumento", vacio, vacio);
            ts.Insertar("CELLO",       CELLO,       "instrumento", vacio, vacio);
            ts.Insertar("OBOE",        OBOE,        "instrumento", vacio, vacio);

            // ── Sentencias de programacion ──
            ts.Insertar("FIN",      FIN_KW,    "pclave",  vacio, vacio);
            ts.Insertar("DECLARAR", DECLARAR,  "pclave",  vacio, vacio);
            ts.Insertar("MOSTRAR",  MOSTRAR,   "pclave",  vacio, vacio);
            ts.Insertar("INGRESAR", INGRESAR,  "pclave",  vacio, vacio);

            // ── Tipos de dato y literales booleanos ──
            ts.Insertar("entero",   TIPO_ENTERO,   "tipo",    vacio, vacio);
            ts.Insertar("cadena",   TIPO_CADENA,   "tipo",    vacio, vacio);
            ts.Insertar("booleano", TIPO_BOOLEANO, "tipo",    vacio, vacio);
            ts.Insertar("verdadero",VERDADERO,     "literal", vacio, vacio);
            ts.Insertar("falso",    FALSO,         "literal", vacio, vacio);

            // ── Signos de un caracter ──
            ts.Insertar(":",  DOSPUNTOS,   "pclave", vacio, vacio);
            ts.Insertar("|",  BARRA,       "pclave", vacio, vacio);
            ts.Insertar("{",  ALLAVE,      "pclave", vacio, vacio);
            ts.Insertar("}",  CLLAVE,      "pclave", vacio, vacio);
            ts.Insertar("(",  APARENTESIS, "pclave", vacio, vacio);
            ts.Insertar(")",  CPARENTESIS, "pclave", vacio, vacio);
            ts.Insertar("#",  SOSTENIDO,   "pclave", vacio, vacio);
            ts.Insertar("=",  IGUAL,       "pclave", vacio, vacio);
        }

        // ── Verifica si un caracter es un signo del lenguaje ──
        bool esSigno(char c) {
            const char signos[] = ":|{}()#=";
            int k = 0;
            while (signos[k] != '\0') {
                if (signos[k] == c) return true;
                k++;
            }
            return false;
        }

        // ── Metodos de acceso para el analizador sintactico ──
        void   reset()             { i = 0; nLinea = 1; }
        int    getLinea()    const  { return nLinea; }
        string getVariable() const  { return variable; }
        string getNumero()   const  { return numero; }
        string getCadena()   const  { return cadenaStr; }

        // ── Devuelve nombre legible del token ──
        string nombreToken(int tok) {
            switch(tok) {
                case HOJA:         return "HOJA";
                case A4:           return "A4";
                case TITULO:       return "TITULO";
                case AUTOR:        return "AUTOR";
                case PENTAGRAMA:   return "PENTAGRAMA";
                case CLAVE:        return "CLAVE";
                case COMPAS:       return "COMPAS";
                case BIS:          return "BIS";
                case TEMPO:        return "TEMPO";
                case VOLUMEN:      return "VOLUMEN";
                case REPETIR:      return "REPETIR";
                case FIN_REPETIR:  return "FIN_REPETIR";
                case FIN_KW:       return "FIN";
                case DECLARAR:     return "DECLARAR";
                case MOSTRAR:      return "MOSTRAR";
                case INGRESAR:     return "INGRESAR";
                case DO:           return "DO";
                case RE:           return "RE";
                case MI:           return "MI";
                case FA:           return "FA";
                case SOL:          return "SOL";
                case LA:           return "LA";
                case SI:           return "SI";
                case SILENCIO:     return "SILENCIO";
                case NEGRA:        return "NEGRA";
                case BLANCA:       return "BLANCA";
                case REDONDA:      return "REDONDA";
                case CORCHEA:      return "CORCHEA";
                case SEMICORCHEA:  return "SEMICORCHEA";
                case FUSA:         return "FUSA";
                case INSTRUMENTO:  return "INSTRUMENTO";
                case PIANO:        return "PIANO";
                case GUITARRA:     return "GUITARRA";
                case VIOLIN:       return "VIOLIN";
                case FLAUTA:       return "FLAUTA";
                case TROMPETA:     return "TROMPETA";
                case VIOLONCHELO:  return "VIOLONCHELO";
                case CELLO:        return "CELLO";
                case OBOE:         return "OBOE";
                case MAYOR:        return "MAYOR";
                case MENOR:        return "MENOR";
                case DOSPUNTOS:    return "DOSPUNTOS";
                case BARRA:        return "BARRA";
                case ALLAVE:       return "ALLAVE";
                case CLLAVE:       return "CLLAVE";
                case APARENTESIS:  return "APAR";
                case CPARENTESIS:  return "CPAR";
                case SOSTENIDO:    return "SOSTENIDO";
                case BEMOL:        return "BEMOL";
                case IGUAL:        return "IGUAL";
                case CADENA:       return "CADENA";
                case ENTERO:       return "ENTERO";
                case FRACCION:     return "FRACCION";
                case IDENTIFICADOR:return "IDENTIFICADOR";
                case TIPO_ENTERO:  return "TIPO_ENTERO";
                case TIPO_CADENA:  return "TIPO_CADENA";
                case TIPO_BOOLEANO:return "TIPO_BOOLEANO";
                case VERDADERO:    return "VERDADERO";
                case FALSO:        return "FALSO";
                case FIN:          return "FIN";
                case EL1:          return "EL1";
                case EL2:          return "EL2";
                case EL3:          return "EL3";
                case EL4:          return "EL4";
                default:           return "DESCONOCIDO";
            }
        }

        // ── Devuelve categoria del token ──
        string categoriaToken(int tok) {
            if (tok >= 0   && tok <= 15) return "Palabra clave";
            if (tok >= 20  && tok <= 27) return "Nota";
            if (tok >= 30  && tok <= 35) return "Duracion";
            if (tok == 40)               return "Palabra clave";
            if (tok >= 41  && tok <= 48) return "Instrumento";
            if (tok == 50  || tok == 51) return "Modo armonico";
            if (tok >= 60  && tok <= 67) return "Signo";
            if (tok == 70)               return "Cadena literal";
            if (tok == 71)               return "Numero entero";
            if (tok == 72)               return "Fraccion";
            if (tok == IDENTIFICADOR)    return "Identificador";
            if (tok == TIPO_ENTERO  ||
                tok == TIPO_CADENA  ||
                tok == TIPO_BOOLEANO)    return "Tipo de dato";
            if (tok == VERDADERO ||
                tok == FALSO)            return "Literal booleano";
            if (tok == IGUAL)            return "Operador";
            if (tok == FIN)              return "Fin de entrada";
            if (tok >= 901 && tok <= 904)return "ERROR LEXICO";
            return "?";
        }

        // ============================================================
        //  NUCLEO: getToken()  — AFD de 7 estados
        // ============================================================
        // q0: estado inicial / blancos / comentarios
        // q1: identificador o palabra clave
        // q2: entero o fraccion N/M
        // q3: cadena "..."
        // q4: signo de un caracter
        // q5: bemol (letra 'b' minuscula sola)
        // q6: FIN_REPETIR con guion bajo (manejado en q1)
        // ============================================================
        int getToken() {
            // q0 — saltar blancos y comentarios
            while (true) {
                while (cad[i] == ' ' || cad[i] == '\t' || cad[i] == '\r') i++;
                if (cad[i] == '\n') { nLinea++; i++; continue; }
                // Comentario de linea: //
                if (cad[i] == '/' && cad[i+1] == '/') {
                    while (cad[i] != '\n' && cad[i] != '\0') i++;
                    continue;
                }
                break;
            }

            // FIN de entrada
            if (cad[i] == '\0') return FIN;

            // q1 — identificador o palabra clave
            //      (acepta letras, digitos, guion bajo → permite FIN_REPETIR)
            if (isalpha(cad[i]) || cad[i] == '_') {
                // Caso especial: 'b' minuscula sola = BEMOL (q5)
                // Solo si el siguiente caracter NO es alfanumerico ni '_'
                if (cad[i] == 'b' && !isalpha(cad[i+1]) && !isdigit(cad[i+1]) && cad[i+1] != '_') {
                    i++;
                    return BEMOL;
                }

                // -- FIX: limite de tamano del identificador (evita overflow) --
                char tmp[100];
                int  tmp_cont = 0;
                while ((isalpha(cad[i]) || isdigit(cad[i]) || cad[i] == '_')
                       && tmp_cont < (int)sizeof(tmp) - 1) {
                    tmp[tmp_cont++] = cad[i++];
                }
                tmp[tmp_cont] = '\0';

                Atributos attr;
                string lex = tmp;
                variable = tmp;

                // Buscar en tabla de simbolos (solo pclave, nota, duracion, etc.)
                if (ts.Buscar(lex, attr)) {
                    return attr.token;
                }

                // Notacion con bemol pegado: "SIb", "MIb", "REb", etc.
                // Si termina en 'b' y el prefijo es una nota conocida,
                // devolver la nota y "devolver" la 'b' al buffer de entrada.
                if (lex.length() >= 2 && lex.back() == 'b') {
                    string prefijo = lex.substr(0, lex.length() - 1);
                    Atributos attrPref;
                    if (ts.Buscar(prefijo, attrPref) && attrPref.tipo == "nota") {
                        i--;           // retrocede: la 'b' sera el proximo token
                        variable = prefijo;
                        return attrPref.token;
                    }
                }

                // Identificador de usuario (variable, nombre no reconocido)
                return IDENTIFICADOR;
            }

            // q2 — entero o fraccion N/M
            if (isdigit(cad[i])) {
                // -- FIX: limite de tamano del numero (evita overflow) --
                char tmp[50];
                int  tmp_cont = 0;
                while (isdigit(cad[i]) && tmp_cont < (int)sizeof(tmp) - 1) {
                    tmp[tmp_cont++] = cad[i++];
                }
                if (cad[i] == '/' && tmp_cont < (int)sizeof(tmp) - 1) {
                    tmp[tmp_cont++] = cad[i++]; // consume '/'
                    if (!isdigit(cad[i])) {
                        // EL2: fraccion sin denominador
                        tmp[tmp_cont] = '\0';
                        numero = tmp;
                        return EL2;
                    }
                    while (isdigit(cad[i]) && tmp_cont < (int)sizeof(tmp) - 1) {
                        tmp[tmp_cont++] = cad[i++];
                    }
                    tmp[tmp_cont] = '\0';
                    fraccion = tmp;
                    return FRACCION;
                }
                tmp[tmp_cont] = '\0';
                numero = tmp;
                return ENTERO;
            }

            // q3 — cadena literal "..."
            if (cad[i] == '"') {
                i++; // consume '"' de apertura
                // -- FIX: limite de tamano de la cadena (evita overflow) --
                char tmp[500];
                int  tmp_cont = 0;
                while (cad[i] != '"' && cad[i] != '\0'
                       && tmp_cont < (int)sizeof(tmp) - 1) {
                    if (cad[i] == '\n') nLinea++;
                    tmp[tmp_cont++] = cad[i++];
                }
                if (cad[i] == '\0') {
                    // EL1: cadena sin cerrar
                    tmp[tmp_cont] = '\0';
                    cadenaStr = tmp;
                    return EL1;
                }
                i++; // consume '"' de cierre
                tmp[tmp_cont] = '\0';
                cadenaStr = tmp;
                return CADENA;
            }

            // q4 — signos de un caracter
            if (esSigno(cad[i])) {
                char tmp[2] = { cad[i], '\0' };
                Atributos attr;
                string lex = tmp;
                i++;
                if (ts.BuscarPClave(lex, attr)) {
                    return attr.token;
                }
                return EL4;
            }

            // EL4: caracter ilegal (no pertenece al alfabeto del DSL)
            i++;
            return EL4;
        }

        // ============================================================
        //  Lexico() — recorre todos los tokens e imprime tabla
        // ============================================================
        bool Lexico() {
            i       = 0;
            nLinea  = 1;
            contTokens  = 0;
            contErrores = 0;

            cout << "\n" << string(78, '=') << "\n";
            cout << "  ANALISIS LEXICO -- DSL Musical\n";
            cout << string(78, '=') << "\n";
            cout << left
                 << setw(7)  << "LINEA"
                 << setw(6)  << "COD"
                 << setw(16) << "TOKEN"
                 << setw(24) << "LEXEMA"
                 << "CATEGORIA\n";
            cout << string(78, '-') << "\n";

            while (true) {
                int lineaActual = nLinea;
                int tok = getToken();
                contTokens++;

                // Construir lexema visible
                string lexVis;
                switch (tok) {
                    case CADENA:       lexVis = "\"" + cadenaStr + "\""; break;
                    case ENTERO:       lexVis = numero;   break;
                    case FRACCION:     lexVis = fraccion; break;
                    case IDENTIFICADOR:lexVis = variable; break;
                    case EL1:          lexVis = "\"" + cadenaStr; break;
                    case EL2:          lexVis = numero;   break;
                    case EL3:          lexVis = variable; break;
                    case EL4:          lexVis = string(1, cad[i-1]); break;
                    case BEMOL:        lexVis = "b";       break;
                    case FIN:          lexVis = "EOF";     break;
                    default: {
                        // Para palabras clave y signos buscamos el lexema en la tabla
                        Atributos attr;
                        // Buscar por token
                        for (auto item : ts.getTabla()) {
                            if (item.token == tok) { lexVis = item.lexema; break; }
                        }
                        if (lexVis.empty()) lexVis = variable;
                        break;
                    }
                }

                bool esError = (tok >= EL1 && tok <= EL4);
                if (esError) contErrores++;

                cout << left
                     << setw(7)  << lineaActual
                     << setw(6)  << tok
                     << setw(16) << nombreToken(tok)
                     << setw(24) << lexVis
                     << categoriaToken(tok);

                // Mensaje de error especifico
                if (esError) {
                    switch (tok) {
                        case EL1: cout << "  -> cadena iniciada con '\"' sin cierre"; break;
                        case EL2: cout << "  -> fraccion '" << lexVis << "' sin denominador"; break;
                        case EL3: cout << "  -> '" << lexVis << "' no pertenece al lexico DSL"; break;
                        case EL4: cout << "  -> '" << lexVis << "' es un simbolo no permitido"; break;
                    }
                }
                cout << "\n";

                if (tok == FIN) break;

                if (esError) {
                    // No detener el analisis; modo panico: continuar
                }
            }

            cout << string(78, '=') << "\n";
            cout << "  Tokens reconocidos : " << (contTokens - 1) << "\n";
            cout << "  Errores lexicos    : " << contErrores << "\n";
            cout << "  Lineas procesadas  : " << nLinea << "\n";
            if (contErrores == 0)
                cout << "  OK  Analisis completado sin errores.\n";
            else
                cout << "  Se encontraron " << contErrores << " error(es). Revisar codigo fuente.\n";
            cout << string(78, '=') << "\n\n";

            return (contErrores == 0);
        }

        bool leerArchivo(const char direccion[]) {
            ifstream archivo(direccion);
            if (!archivo.is_open()) return false;
            int k = 0;
            char c;
            // -- FIX: limite acorde al nuevo tamano de cad (MAX_CAD) --
            while (archivo.get(c) && k < MAX_CAD - 2) {
                cad[k++] = c;
            }
            cad[k] = '\0';
            archivo.close();
            return true;
        }

        // Mostrar codigo fuente numerado (igual al estilo del docente)
        void MostrarFuente() {
            cout << "\n  -- Codigo fuente DSL ------------------------------------\n";
            int linea = 1;
            int k     = 0;
            cout << "  " << setw(3) << linea << " | ";
            while (cad[k] != '\0') {
                if (cad[k] == '\n') {
                    linea++;
                    cout << "\n  " << setw(3) << linea << " | ";
                } else {
                    cout << cad[k];
                }
                k++;
            }
            cout << "\n  ---------------------------------------------------------\n";
        }
};

#endif // ANALIZADOR_LEXICO_H
