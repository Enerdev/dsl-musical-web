#ifndef ANALIZADOR_SINTACTICO_H
#define ANALIZADOR_SINTACTICO_H

#include "analizador_lexico.h"
#include "analizador_semantico.h"
#include "ast.h"
#include <stack>

// ============================================================
//  ANALIZADOR SINTACTICO -- AFD con tabla de transiciones
//  tTransicion[estado][token] + pila de retorno para bloques
//  anidados (BIS / REPETIR / SI), ya que la gramatica del DSL
//  no es regular pura (tiene anidamiento ilimitado).
//
//  Estilo: Atributos / TablaSimbolos / Analisis (igual al resto
//  del proyecto). Token #define se toman de analizador_lexico.h
// ============================================================

// ── Estados del AFD sintactico ─────────────────────────────
const int S_INICIO        = 0;
const int S_HOJA          = 1;
const int S_PRINCIPAL     = 2;   // dispatcher central (topItems)
const int S_TITULO        = 3;
const int S_TITULO_CAD    = 4;
const int S_AUTOR         = 5;
const int S_AUTOR_CAD     = 6;

const int S_PENT          = 10;
const int S_PENT_NUM      = 11;

const int S_CLAVE         = 20;
const int S_CLAVE_VAL     = 21;

const int S_COMPAS        = 22;
const int S_COMPAS_VAL    = 23;

const int S_TEMPO         = 24;
const int S_TEMPO_VAL     = 25;

const int S_VOLUMEN       = 26;
const int S_VOLUMEN_VAL   = 27;

const int S_INSTR         = 28;
const int S_INSTR_VAL     = 29;

const int S_NOTA          = 30;
const int S_NOTA_ALT      = 31;
const int S_NOTA_DOSP     = 32;
const int S_NOTA_DUR      = 33;

const int S_BIS           = 40;
const int S_BIS_ALLAVE    = 41;
const int S_BIS_CLLAVE    = 42;

const int S_REP           = 50;
const int S_REP_APAR      = 51;
const int S_REP_NUM       = 52;
const int S_REP_CPAR      = 53;
const int S_REP_ALLAVE    = 54;
const int S_REP_CLLAVE    = 55;

const int S_DECL          = 60;
const int S_DECL_ID       = 61;
const int S_DECL_DOSP     = 62;
const int S_DECL_TIPO     = 63;

const int S_ASIG_ID       = 70;
const int S_ASIG_IGUAL    = 71;
const int S_ASIG_VAL      = 72;

const int S_MOSTRAR       = 80;
const int S_MOSTRAR_VAL   = 81;

const int S_INGRESAR      = 90;
const int S_INGRESAR_ID   = 91;

const int S_SI            = 100;
const int S_SI_APAR       = 101;
const int S_SI_ID         = 102;
const int S_SI_CPAR       = 103;
const int S_SI_ALLAVE     = 104;
const int S_SI_CLLAVE     = 105;

const int S_ACEPTACION    = 200;
const int S_ERROR         = -1;

const int MAX_ESTADOS = 210;
const int MAX_TOKEN   = 1000;   // cubre 0..904 (EL4)

// ── Estados "dispatcher" desde donde se lanzan sub-reglas ──
inline bool esDispatcher(int e) {
    return e == S_PRINCIPAL || e == S_BIS_ALLAVE ||
           e == S_REP_ALLAVE || e == S_SI_ALLAVE;
}

// ============================================================
//  CLASE: AnalizadorSintactico
// ============================================================
class AnalizadorSintactico {
private:
    AnalizadorLexico *lexer;
    int  tTransicion[MAX_ESTADOS][MAX_TOKEN];

    int  estado;
    int  tokenActual;
    int  tokenLookahead;
    bool hayLookahead;
    int  erroresSint;
    std::stack<int> pila;     // pila de dispatchers padre (para volver al cerrar un bloque)
    int  dispatcherActual;    // dispatcher vigente: S_PRINCIPAL o el *_ALLAVE del bloque actual

    // ── Instancia del analizador semantico ──
    AnalizadorSemantico sem;

    // AST
    NodoPrograma *astRoot;
    std::stack<NodoContenedor*> astStack;
    string notaActual;
    string notaAlteracion;
    string notaDuracion;
    int lineaNota;
    string siCondicion;
    int repeticiones;

    // LHS y RHS temporales para validaciones
    string varAsignacion;
    int lineaAsignacion;
    string varDeclarada;
    int lineaDeclarada;

    // ── Avance / lookahead (igual estilo que el lexer wrapper original) ──
    void avanzar() {
        if (hayLookahead) {
            tokenActual  = tokenLookahead;
            hayLookahead = false;
        } else {
            tokenActual = lexer->getToken();
        }
    }

    int peek() {
        if (!hayLookahead) {
            tokenLookahead = lexer->getToken();
            hayLookahead   = true;
        }
        return tokenLookahead;
    }

    void fillRange(int est, int tokIni, int tokFin, int destino) {
        for (int t = tokIni; t <= tokFin; t++) tTransicion[est][t] = destino;
    }

    NodoContenedor *astPadreActual() {
        return astStack.empty() ? nullptr : astStack.top();
    }

    void agregarNodoAST(Nodo *nodo) {
        NodoContenedor *padre = astPadreActual();
        if (padre) padre->agregarHijo(nodo);
        else delete nodo;
    }

    void empujarNodoAST(NodoContenedor *nodo) {
        agregarNodoAST(nodo);
        astStack.push(nodo);
    }

    void popNodoAST() {
        if (!astStack.empty()) astStack.pop();
    }

    void initAST() {
        if (astRoot) delete astRoot;
        astRoot = new NodoPrograma();
        while (!astStack.empty()) astStack.pop();
        astStack.push(astRoot);
        resetNoteInfo();
        siCondicion.clear();
        repeticiones = 0;
    }

    // ── Construccion de la matriz de transiciones ───────────
    void construirTabla() {
        for (int e = 0; e < MAX_ESTADOS; e++)
            for (int t = 0; t < MAX_TOKEN; t++)
                tTransicion[e][t] = S_ERROR;

        // Cabecera
        tTransicion[S_INICIO][HOJA]      = S_HOJA;
        tTransicion[S_HOJA][A4]          = S_PRINCIPAL;
        tTransicion[S_PRINCIPAL][TITULO] = S_TITULO;
        tTransicion[S_TITULO][CADENA]    = S_TITULO_CAD;
        tTransicion[S_PRINCIPAL][AUTOR]  = S_AUTOR;
        tTransicion[S_AUTOR][CADENA]     = S_AUTOR_CAD;

        // PENTAGRAMA n
        tTransicion[S_PRINCIPAL][PENTAGRAMA] = S_PENT;
        tTransicion[S_PENT][ENTERO]          = S_PENT_NUM;

        // CLAVE val
        tTransicion[S_PRINCIPAL][CLAVE] = S_CLAVE;
        fillRange(S_CLAVE, DO, SI, S_CLAVE_VAL);
        tTransicion[S_CLAVE][MAYOR] = S_CLAVE_VAL;
        tTransicion[S_CLAVE][MENOR] = S_CLAVE_VAL;

        // COMPAS frac|entero
        tTransicion[S_PRINCIPAL][COMPAS] = S_COMPAS;
        tTransicion[S_COMPAS][FRACCION]  = S_COMPAS_VAL;
        tTransicion[S_COMPAS][ENTERO]    = S_COMPAS_VAL;

        // TEMPO ent / VOLUMEN ent
        tTransicion[S_PRINCIPAL][TEMPO]   = S_TEMPO;
        tTransicion[S_TEMPO][ENTERO]      = S_TEMPO_VAL;
        tTransicion[S_PRINCIPAL][VOLUMEN] = S_VOLUMEN;
        tTransicion[S_VOLUMEN][ENTERO]    = S_VOLUMEN_VAL;

        // INSTRUMENTO inst
        tTransicion[S_PRINCIPAL][INSTRUMENTO] = S_INSTR;
        fillRange(S_INSTR, PIANO, OBOE, S_INSTR_VAL);
        tTransicion[S_INSTR][IDENTIFICADOR] = S_INSTR_VAL;

        // notaExpr: (nota|SILENCIO)[#|b] : duracion  (repetible -> S_NOTA)
        fillRange(S_PRINCIPAL,  DO, SILENCIO, S_NOTA);
        fillRange(S_BIS_ALLAVE, DO, SILENCIO, S_NOTA);
        fillRange(S_REP_ALLAVE, DO, SILENCIO, S_NOTA);
        fillRange(S_NOTA_DUR,   DO, SILENCIO, S_NOTA);   // encadena varias notas
        tTransicion[S_NOTA][SOSTENIDO]  = S_NOTA_ALT;
        tTransicion[S_NOTA][BEMOL]      = S_NOTA_ALT;
        tTransicion[S_NOTA][DOSPUNTOS]  = S_NOTA_DOSP;
        tTransicion[S_NOTA_ALT][DOSPUNTOS] = S_NOTA_DOSP;
        fillRange(S_NOTA_DOSP, NEGRA, FUSA, S_NOTA_DUR);
        tTransicion[S_NOTA_DUR][BARRA]  = S_PRINCIPAL;   // destino "logico"; el retorno real lo decide la pila

        // BIS { notaCompas* }
        tTransicion[S_PRINCIPAL][BIS] = S_BIS;
        tTransicion[S_BIS][ALLAVE]    = S_BIS_ALLAVE;
        tTransicion[S_BIS_ALLAVE][CLLAVE] = S_BIS_CLLAVE;

        // REPETIR [(ent)] { ... } FIN|FIN_REPETIR
        tTransicion[S_PRINCIPAL][REPETIR]   = S_REP;
        tTransicion[S_REP][APARENTESIS]     = S_REP_APAR;
        tTransicion[S_REP_APAR][ENTERO]     = S_REP_NUM;
        tTransicion[S_REP_NUM][CPARENTESIS] = S_REP_CPAR;
        tTransicion[S_REP_CPAR][ALLAVE]     = S_REP_ALLAVE;
        tTransicion[S_REP][ALLAVE]          = S_REP_ALLAVE;
        tTransicion[S_REP_ALLAVE][REPETIR]  = S_REP;          // REPETIR anidado
        tTransicion[S_REP_ALLAVE][CLLAVE]   = S_REP_CLLAVE;
        tTransicion[S_REP_CLLAVE][FIN_KW]      = S_PRINCIPAL;
        tTransicion[S_REP_CLLAVE][FIN_REPETIR] = S_PRINCIPAL;

        // DECLARAR id : tipo
        tTransicion[S_PRINCIPAL][DECLARAR]  = S_DECL;
        tTransicion[S_BIS_ALLAVE][DECLARAR] = S_DECL;
        tTransicion[S_REP_ALLAVE][DECLARAR] = S_DECL;
        tTransicion[S_SI_ALLAVE][DECLARAR]  = S_DECL;
        tTransicion[S_DECL][IDENTIFICADOR]  = S_DECL_ID;
        tTransicion[S_DECL_ID][DOSPUNTOS]   = S_DECL_DOSP;
        fillRange(S_DECL_DOSP, TIPO_ENTERO, TIPO_BOOLEANO, S_DECL_TIPO);

        // asignacion id = valor
        tTransicion[S_PRINCIPAL][IDENTIFICADOR]  = S_ASIG_ID;
        tTransicion[S_BIS_ALLAVE][IDENTIFICADOR] = S_ASIG_ID;
        tTransicion[S_REP_ALLAVE][IDENTIFICADOR] = S_ASIG_ID;
        tTransicion[S_SI_ALLAVE][IDENTIFICADOR]  = S_ASIG_ID;
        tTransicion[S_ASIG_ID][IGUAL]            = S_ASIG_IGUAL;
        tTransicion[S_ASIG_IGUAL][ENTERO]        = S_ASIG_VAL;
        tTransicion[S_ASIG_IGUAL][CADENA]        = S_ASIG_VAL;
        tTransicion[S_ASIG_IGUAL][VERDADERO]     = S_ASIG_VAL;
        tTransicion[S_ASIG_IGUAL][FALSO]         = S_ASIG_VAL;
        tTransicion[S_ASIG_IGUAL][IDENTIFICADOR] = S_ASIG_VAL;

        // MOSTRAR valor
        tTransicion[S_PRINCIPAL][MOSTRAR]  = S_MOSTRAR;
        tTransicion[S_BIS_ALLAVE][MOSTRAR] = S_MOSTRAR;
        tTransicion[S_REP_ALLAVE][MOSTRAR] = S_MOSTRAR;
        tTransicion[S_SI_ALLAVE][MOSTRAR]  = S_MOSTRAR;
        tTransicion[S_MOSTRAR][ENTERO]        = S_MOSTRAR_VAL;
        tTransicion[S_MOSTRAR][CADENA]        = S_MOSTRAR_VAL;
        tTransicion[S_MOSTRAR][VERDADERO]     = S_MOSTRAR_VAL;
        tTransicion[S_MOSTRAR][FALSO]         = S_MOSTRAR_VAL;
        tTransicion[S_MOSTRAR][IDENTIFICADOR] = S_MOSTRAR_VAL;

        // INGRESAR id
        tTransicion[S_PRINCIPAL][INGRESAR]  = S_INGRESAR;
        tTransicion[S_BIS_ALLAVE][INGRESAR] = S_INGRESAR;
        tTransicion[S_REP_ALLAVE][INGRESAR] = S_INGRESAR;
        tTransicion[S_SI_ALLAVE][INGRESAR]  = S_INGRESAR;
        tTransicion[S_INGRESAR][IDENTIFICADOR] = S_INGRESAR_ID;

        // SI (id) { sentencias } FIN
        tTransicion[S_PRINCIPAL][SI]  = S_SI;
        tTransicion[S_SI][APARENTESIS] = S_SI_APAR;
        tTransicion[S_SI_APAR][IDENTIFICADOR] = S_SI_ID;
        tTransicion[S_SI_ID][CPARENTESIS]     = S_SI_CPAR;
        tTransicion[S_SI_CPAR][ALLAVE]        = S_SI_ALLAVE;
        tTransicion[S_SI_ALLAVE][CLLAVE]      = S_SI_CLLAVE;
        tTransicion[S_SI_CLLAVE][FIN_KW]      = S_PRINCIPAL;

        // Fin de programa
        tTransicion[S_PRINCIPAL][FIN] = S_ACEPTACION;
    }

    // ── Predicados auxiliares (igual a tu version anterior) ──
    bool esErrorLexico(int t) { return t >= EL1 && t <= EL4; }

    void errorSint(const string &msg) {
        erroresSint++;
        cout << "  [ERROR SINT L" << lexer->getLinea() << "] " << msg << "\n";
    }

    // Sincroniza en modo panico hasta un token "seguro"
    void recuperar() {
        while (tokenActual != BARRA      && tokenActual != FIN      &&
               tokenActual != PENTAGRAMA && tokenActual != CLLAVE   &&
               tokenActual != FIN_REPETIR && tokenActual != FIN_KW) {
            if (tokenActual == FIN) break;
            avanzar();
        }
    }

    // Mensaje de [OK] al completar una sub-regla (cosmetico, igual estilo)
    void anunciar(int estadoFinal) {
        switch (estadoFinal) {
            case S_AUTOR_CAD:  cout << "  [OK] Cabecera\n"; break;
            case S_PENT_NUM:   cout << "  [OK] Pentagrama " << lexer->getNumero() << "\n"; break;
            case S_DECL_TIPO:  cout << "  [OK] DECLARAR " << lexer->getVariable() << "\n"; break;
            case S_ASIG_VAL:   cout << "  [OK] Asignacion " << lexer->getVariable() << " = ...\n"; break;
            case S_MOSTRAR_VAL:cout << "  [OK] MOSTRAR\n"; break;
            case S_INGRESAR_ID:cout << "  [OK] INGRESAR " << lexer->getVariable() << "\n"; break;
            case S_BIS_CLLAVE: cout << "  [OK] Bloque BIS\n"; break;
            case S_REP_CLLAVE: cout << "  [OK] Bloque REPETIR\n"; break;
            case S_SI_CLLAVE:  cout << "  [OK] Condicional SI\n"; break;
            default: break;
        }
    }

    void resetNoteInfo() {
        notaActual.clear();
        notaAlteracion.clear();
        notaDuracion.clear();
        lineaNota = 0;
    }

    void empezarNota(int token) {
        if (token >= DO && token <= SILENCIO && token != SI) {
            notaActual = lexer->getVariable();
            lineaNota = lexer->getLinea();
            notaAlteracion.clear();
            notaDuracion.clear();
        }
    }

    void aplicarAlteracion(int token) {
        if (token == SOSTENIDO) notaAlteracion = "#";
        else if (token == BEMOL) notaAlteracion = "b";
    }

    void asignarDuracion(int token) {
        notaDuracion = lexer->nombreToken(token);
    }

    void crearNotaAST() {
        if (notaActual.empty() || notaDuracion.empty()) return;
        NodoNota *nodo = new NodoNota(notaActual, notaAlteracion, notaDuracion, lineaNota);
        agregarNodoAST(nodo);
        resetNoteInfo();
    }

    // ¿Este estado, al alcanzarse, cierra una sub-regla y debe
    // resolverse con la pila de retorno?
    bool esEstadoDeRetorno(int e) {
        return e == S_AUTOR_CAD   || e == S_TITULO_CAD  ||
               e == S_PENT_NUM    || e == S_CLAVE_VAL    ||
               e == S_COMPAS_VAL  || e == S_TEMPO_VAL    ||
               e == S_VOLUMEN_VAL || e == S_INSTR_VAL    ||
               e == S_DECL_TIPO   || e == S_ASIG_VAL     ||
               e == S_MOSTRAR_VAL || e == S_INGRESAR_ID;
    }

public:
    AnalizadorSintactico(AnalizadorLexico *lex)
        : lexer(lex), estado(S_INICIO), tokenActual(-1), tokenLookahead(-1),
          hayLookahead(false), erroresSint(0),
          astRoot(nullptr), notaActual(""), notaAlteracion(""), notaDuracion(""), lineaNota(0),
          siCondicion(""), repeticiones(0),
          varAsignacion(""), lineaAsignacion(0), varDeclarada(""), lineaDeclarada(0) {
        construirTabla();
        initAST();
    }

    ~AnalizadorSintactico() {
        delete astRoot;
    }

    bool Sintactico() {
        initAST();
        lexer->reset();
        hayLookahead = false;
        erroresSint  = 0;
        sem.reset();
        estado       = S_INICIO;
        dispatcherActual = S_PRINCIPAL;
        while (!pila.empty()) pila.pop();

        avanzar();

        cout << "\n" << string(78, '=') << "\n";
        cout << "  ANALISIS SINTACTICO -- DSL Musical (tabla de transiciones)\n";
        cout << string(78, '=') << "\n";
        cout << left << setw(14) << "ESTADO_ANT" << setw(16) << "TOKEN"
             << setw(14) << "ESTADO_SIG" << "RESULTADO\n";
        cout << string(78, '-') << "\n";

        // Bucle principal: motor table-driven.
        // dispatcherActual = el estado "despachador" vigente (S_PRINCIPAL en
        // la raiz, o el *_ALLAVE del bloque BIS/REPETIR/SI en que estamos).
        // pila = guarda el dispatcherActual del nivel padre, para
        // restaurarlo cuando el bloque actual se cierra (anidamiento).
        while (estado != S_ACEPTACION) {

            // 1) Token de error lexico: se reporta en el lexico, aqui se salta
            if (esErrorLexico(tokenActual)) {
                avanzar();
                continue;
            }

            int origen  = estado;
            int destino;

            if (origen == S_NOTA_DUR) {
                crearNotaAST();
            }

            // Guardar identificadores de usuario antes de avanzar/perderlos
            if (esDispatcher(origen) && tokenActual == IDENTIFICADOR) {
                varAsignacion = lexer->getVariable();
                lineaAsignacion = lexer->getLinea();
            }
            if (origen == S_DECL && tokenActual == IDENTIFICADOR) {
                varDeclarada = lexer->getVariable();
                lineaDeclarada = lexer->getLinea();
            }
            if (origen == S_SI_APAR && tokenActual == IDENTIFICADOR) {
                siCondicion = lexer->getVariable();
            }
            if (origen == S_REP_NUM && tokenActual == ENTERO) {
                repeticiones = stoi(lexer->getNumero());
            }

            // Token SI es ambiguo: nota musical (SI:NEGRA) o condicional
            // SI(id){...}. Una sola celda de tabla no puede resolverlo
            // porque ambos comparten el mismo codigo de token (26);
            // se decide en tiempo de ejecucion con lookahead.
            if (esDispatcher(estado) && tokenActual == SI) {
                destino = (peek() == APARENTESIS) ? S_SI : S_NOTA;
            } else {
                destino = tTransicion[estado][tokenActual];
            }

            if (destino == S_NOTA) {
                empezarNota(tokenActual);
            }
            if (origen == S_NOTA && (tokenActual == SOSTENIDO || tokenActual == BEMOL)) {
                aplicarAlteracion(tokenActual);
            }
            if (origen == S_NOTA_DOSP && tokenActual >= NEGRA && tokenActual <= FUSA) {
                asignarDuracion(tokenActual);
            }
            // SI(id){...}. Una sola celda de tabla no puede resolverlo
            // porque ambos comparten el mismo codigo de token (26);
            // se decide en tiempo de ejecucion con lookahead.
            if (esDispatcher(estado) && tokenActual == SI) {
                destino = (peek() == APARENTESIS) ? S_SI : S_NOTA;
            } else {
                destino = tTransicion[estado][tokenActual];
            }

            if (destino == S_ERROR) {
                // Cierre natural de FIN de archivo dentro de un dispatcher:
                // si quedan bloques sin cerrar, los reportamos y cerramos.
                if (esDispatcher(estado) && tokenActual == FIN) {
                    if (!pila.empty()) {
                        errorSint("Bloque sin cerrar (falta '}' o FIN)");
                        dispatcherActual = pila.top(); pila.pop();
                        estado = dispatcherActual;
                        continue;
                    }
                    estado = S_ACEPTACION;
                    break;
                }
                errorSint("Token inesperado '" + lexer->nombreToken(tokenActual) +
                          "' en estado " + to_string(origen));
                cout << left << setw(14) << origen
                     << setw(16) << lexer->nombreToken(tokenActual)
                     << setw(14) << "--" << "ERROR\n";
                int tokAntes = tokenActual;
                recuperar();
                if (tokenActual == tokAntes) avanzar(); // garantiza progreso (evita bucle infinito)
                if (tokenActual == FIN) { estado = S_ACEPTACION; break; }
                estado = dispatcherActual; // vuelve al dispatcher vigente (evita cascada de errores falsos)
                continue; // reintenta desde el dispatcher tras sincronizar
            }

            // --- CONTROLES SEMANTICOS ---
            // 1. Registro de declaracion: DECLARAR id : tipo
            if (origen == S_DECL_DOSP && (tokenActual == TIPO_ENTERO || tokenActual == TIPO_CADENA || tokenActual == TIPO_BOOLEANO)) {
                string tipoStr = "";
                if (tokenActual == TIPO_ENTERO) tipoStr = "entero";
                else if (tokenActual == TIPO_CADENA) tipoStr = "cadena";
                else if (tokenActual == TIPO_BOOLEANO) tipoStr = "booleano";

                sem.declararVariable(varDeclarada, tipoStr, lineaDeclarada);
                agregarNodoAST(new NodoDeclaracion(varDeclarada, tipoStr, lineaDeclarada));
            }

            // 2. Asignacion: id = valor
            if (origen == S_ASIG_IGUAL && (tokenActual == ENTERO || tokenActual == CADENA || tokenActual == VERDADERO || tokenActual == FALSO || tokenActual == IDENTIFICADOR)) {
                string tipoRHS = "";
                string rhsValName = "";
                int rhsValLinea = lexer->getLinea();
                bool rhsEsVariable = false;
                string rhsValor;

                if (tokenActual == ENTERO) {
                    tipoRHS = "entero";
                    rhsValor = lexer->getNumero();
                } else if (tokenActual == CADENA) {
                    tipoRHS = "cadena";
                    rhsValor = lexer->getCadena();
                } else if (tokenActual == VERDADERO || tokenActual == FALSO) {
                    tipoRHS = "booleano";
                    rhsValor = lexer->nombreToken(tokenActual);
                } else if (tokenActual == IDENTIFICADOR) {
                    tipoRHS = ""; // Se resolverá dentro del AnalizadorSemantico
                    rhsValName = lexer->getVariable();
                    rhsEsVariable = true;
                    rhsValor = rhsValName;
                }

                sem.asignarVariable(varAsignacion, lineaAsignacion, tipoRHS, rhsValName, rhsValLinea, rhsEsVariable);
                agregarNodoAST(new NodoAsignacion(varAsignacion, rhsValor, rhsEsVariable, lineaAsignacion));
            }

            // 3. MOSTRAR valor
            if (origen == S_MOSTRAR && (tokenActual == IDENTIFICADOR || tokenActual == ENTERO || tokenActual == CADENA || tokenActual == VERDADERO || tokenActual == FALSO)) {
                if (tokenActual == IDENTIFICADOR) {
                    sem.validarMostrar(lexer->getVariable(), lexer->getLinea());
                    agregarNodoAST(new NodoMostrar(lexer->getVariable(), true, lexer->getLinea()));
                } else {
                    string valor = (tokenActual == ENTERO) ? lexer->getNumero() :
                                   (tokenActual == CADENA) ? lexer->getCadena() :
                                   lexer->nombreToken(tokenActual);
                    agregarNodoAST(new NodoMostrar(valor, false, lexer->getLinea()));
                }
            }

            // 4. INGRESAR id
            if (origen == S_INGRESAR && tokenActual == IDENTIFICADOR) {
                sem.validarIngresar(lexer->getVariable(), lexer->getLinea());
                agregarNodoAST(new NodoIngresar(lexer->getVariable(), lexer->getLinea()));
            }

            // 5. SI (id)
            if (origen == S_SI_APAR && tokenActual == IDENTIFICADOR) {
                sem.validarCondicionSi(lexer->getVariable(), lexer->getLinea());
            }

            cout << left << setw(14) << origen
                 << setw(16) << lexer->nombreToken(tokenActual)
                 << setw(14) << destino << "OK\n";

            // 2) Si el dispatcher vigente lanza un bloque (BIS/REPETIR/SI),
            //    guardamos el dispatcher padre antes de entrar.
            if (origen == dispatcherActual) {
                if (tokenActual == BIS || tokenActual == REPETIR || destino == S_SI) {
                    pila.push(dispatcherActual);
                }
            }

            // 3) Avanza al siguiente estado
            estado = destino;
            anunciar(estado);

            if (estado == S_BIS_ALLAVE) {
                NodoBis *nodo = new NodoBis();
                empujarNodoAST(nodo);
                dispatcherActual = estado;
            }
            if (estado == S_REP_ALLAVE) {
                NodoRepetir *nodo = new NodoRepetir();
                nodo->repeticiones = repeticiones;
                empujarNodoAST(nodo);
                dispatcherActual = estado;
            }
            if (estado == S_SI_ALLAVE) {
                NodoSi *nodo = new NodoSi();
                nodo->condicion = siCondicion;
                empujarNodoAST(nodo);
                dispatcherActual = estado;
            }

            avanzar();

            // 4) Al llegar al *_ALLAVE propio de un bloque recien abierto,
            //    ese pasa a ser el dispatcher vigente.
            if (estado == S_BIS_ALLAVE || estado == S_REP_ALLAVE || estado == S_SI_ALLAVE) {
                dispatcherActual = estado;
            }

            // 5) BIS cierra solo con '}' (no exige FIN). Restaurar dispatcher
            //    padre de inmediato.
            if (estado == S_BIS_CLLAVE) {
                popNodoAST();
                dispatcherActual = pila.empty() ? S_PRINCIPAL : pila.top();
                if (!pila.empty()) pila.pop();
                estado = dispatcherActual;
            }

            // 6) Cierre de REPETIR ({...} FIN|FIN_REPETIR) y SI ({...} FIN):
            //    al volver a S_PRINCIPAL desde *_CLLAVE, restaurar dispatcher padre.
            if ((origen == S_REP_CLLAVE || origen == S_SI_CLLAVE) && destino == S_PRINCIPAL) {
                popNodoAST();
                dispatcherActual = pila.empty() ? S_PRINCIPAL : pila.top();
                if (!pila.empty()) pila.pop();
                estado = dispatcherActual;
            }

            // 7) Sub-reglas simples (CLAVE, COMPAS, TEMPO, VOLUMEN, INSTRUMENTO,
            //    PENTAGRAMA, DECLARAR, asignacion, MOSTRAR, INGRESAR) regresan
            //    al dispatcher vigente, no siempre a S_PRINCIPAL.
            if (esEstadoDeRetorno(estado)) {
                estado = dispatcherActual;
            }

            // 8) Cierre de un compas (BARRA): la tabla apunta logicamente a
            //    S_PRINCIPAL, pero el retorno real es el dispatcher vigente
            //    (permite seguir agregando compases dentro de un bloque).
            if (origen == S_NOTA_DUR && destino == S_PRINCIPAL) {
                estado = dispatcherActual;
            }
        }

        // Mostrar tabla de simbolos de usuario
        sem.mostrarTabla();

        cout << string(78, '-') << "\n";
        cout << "  Errores sintacticos    : " << erroresSint << "\n";
        cout << "  Errores semanticos     : " << sem.getErroresSem() << "\n";
        
        if (erroresSint == 0 && sem.getErroresSem() == 0) {
            cout << "  OK  Analisis sintactico y semantico completado sin errores.\n";
            cout << "\n  AST generado:\n";
            astRoot->imprimir(1);
        } else {
            if (erroresSint > 0 && sem.getErroresSem() > 0)
                cout << "  Se encontraron errores sintacticos y semanticos. Revisar codigo.\n";
            else if (erroresSint > 0)
                cout << "  Se encontraron " << erroresSint << " error(es) sintactico(s). Revisar codigo.\n";
            else
                cout << "  Se encontraron " << sem.getErroresSem() << " error(es) semantico(s). Revisar codigo.\n";
        }
        cout << string(78, '=') << "\n\n";

        return (erroresSint == 0 && sem.getErroresSem() == 0);
    }
};

#endif // ANALIZADOR_SINTACTICO_H
