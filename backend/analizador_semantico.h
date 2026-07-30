#ifndef ANALIZADOR_SEMANTICO_H
#define ANALIZADOR_SEMANTICO_H

#include <iostream>
#include <string>
#include <map>
#include <iomanip>

using namespace std;

// Estructura para almacenar la información de cada símbolo (variable)
struct InfoSimbolo {
    string tipo;       // "entero", "cadena", "booleano"
    int linea;         // Línea de declaración
    bool inicializada; // Indica si ya se le asignó un valor
};

class AnalizadorSemantico {
private:
    std::map<string, InfoSimbolo> tablaSimbolos;
    int erroresSem;

public:
    AnalizadorSemantico() : erroresSem(0) {}

    void reset() {
        tablaSimbolos.clear();
        erroresSem = 0;
    }

    int getErroresSem() const { return erroresSem; }

    void errorSem(const string &codigo, int linea, const string &msg) {
        erroresSem++;
        cout << "  [ERROR SEMANTICO L" << linea << "] " << codigo << ": " << msg << "\n";
    }

    // Registrar una declaración de variable
    // ES3: Redeclaración
    void declararVariable(const string &name, const string &tipo, int linea) {
        if (tablaSimbolos.find(name) != tablaSimbolos.end()) {
            errorSem("ES3", linea, "Redeclaracion de la variable '" + name + "' (declarada originalmente en linea " + to_string(tablaSimbolos[name].linea) + ")");
        } else {
            tablaSimbolos[name] = {tipo, linea, false};
        }
    }

    // Validar y procesar una asignación (id = valor)
    // ES1: Variable no declarada (LHS o RHS)
    // ES4: Uso antes de inicializar (RHS variable)
    // ES2: Tipo incompatible
    void asignarVariable(const string &lhsName, int lhsLinea, const string &rhsTipo, const string &rhsValName, int rhsValLinea, bool rhsEsVariable) {
        // Validar LHS
        if (tablaSimbolos.find(lhsName) == tablaSimbolos.end()) {
            errorSem("ES1", lhsLinea, "Variable '" + lhsName + "' no declarada");
            return;
        }

        string tipoRHS = rhsTipo;
        bool RHS_ok = true;

        if (rhsEsVariable) {
            if (tablaSimbolos.find(rhsValName) == tablaSimbolos.end()) {
                errorSem("ES1", rhsValLinea, "Variable '" + rhsValName + "' no declarada");
                RHS_ok = false;
            } else {
                if (!tablaSimbolos[rhsValName].inicializada) {
                    errorSem("ES4", rhsValLinea, "Uso de variable '" + rhsValName + "' antes de inicializar");
                }
                tipoRHS = tablaSimbolos[rhsValName].tipo;
            }
        }

        if (RHS_ok && tipoRHS != "") {
            if (tablaSimbolos[lhsName].tipo != tipoRHS) {
                errorSem("ES2", rhsValLinea, "Tipo incompatible en asignacion a '" + lhsName + "' (se esperaba '" + tablaSimbolos[lhsName].tipo + "', se obtuvo '" + tipoRHS + "')");
            }
        }

        // Marcar LHS como inicializada en cualquier caso (para evitar cascada de errores de inicialización)
        tablaSimbolos[lhsName].inicializada = true;
    }

    // Validar uso en MOSTRAR
    // ES1: Variable no declarada
    // ES4: Uso antes de inicializar
    void validarMostrar(const string &name, int linea) {
        if (tablaSimbolos.find(name) == tablaSimbolos.end()) {
            errorSem("ES1", linea, "Variable '" + name + "' no declarada");
        } else if (!tablaSimbolos[name].inicializada) {
            errorSem("ES4", linea, "Uso de variable '" + name + "' antes de inicializar");
        }
    }

    // Validar e inicializar en INGRESAR
    // ES1: Variable no declarada
    void validarIngresar(const string &name, int linea) {
        if (tablaSimbolos.find(name) == tablaSimbolos.end()) {
            errorSem("ES1", linea, "Variable '" + name + "' no declarada");
        } else {
            tablaSimbolos[name].inicializada = true;
        }
    }

    // Validar condición de un SI
    // ES1: Variable no declarada
    // ES4: Uso antes de inicializar
    // ES2: Condición no es booleana
    void validarCondicionSi(const string &name, int linea) {
        if (tablaSimbolos.find(name) == tablaSimbolos.end()) {
            errorSem("ES1", linea, "Variable '" + name + "' no declarada");
        } else {
            if (!tablaSimbolos[name].inicializada) {
                errorSem("ES4", linea, "Uso de variable '" + name + "' antes de inicializar");
            }
            if (tablaSimbolos[name].tipo != "booleano") {
                errorSem("ES2", linea, "Tipo incompatible en condicion de SI (se esperaba 'booleano', se obtuvo '" + tablaSimbolos[name].tipo + "')");
            }
        }
    }

    // Mostrar tabla de símbolos
    void mostrarTabla() {
        if (tablaSimbolos.empty()) {
            cout << "\n  No se declararon variables de usuario.\n";
            return;
        }
        cout << "\n" << string(78, '=') << "\n";
        cout << "  TABLA DE SIMBOLOS (Variables de Usuario)\n";
        cout << string(78, '=') << "\n";
        cout << left << setw(20) << "VARIABLE"
             << setw(16) << "TIPO"
             << setw(16) << "LINEA DECL."
             << "INICIALIZADA\n";
        cout << string(78, '-') << "\n";
        for (auto const& item : tablaSimbolos) {
            const string &name = item.first;
            const InfoSimbolo &info = item.second;
            cout << left << setw(20) << name
                 << setw(16) << info.tipo
                 << setw(16) << to_string(info.linea)
                 << (info.inicializada ? "si" : "no") << "\n";
        }
        cout << string(78, '=') << "\n";
    }

    std::map<string, InfoSimbolo> getTablaSimbolos() const {
        return tablaSimbolos;
    }
};

#endif // ANALIZADOR_SEMANTICO_H
