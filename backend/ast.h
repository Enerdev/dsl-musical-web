#ifndef AST_H
#define AST_H

#include <iostream>
#include <string>
#include <vector>

using namespace std;

static void imprimirIndent(int nivel) {
    for (int i = 0; i < nivel; i++) cout << "  ";
}

class Nodo {
public:
    virtual ~Nodo() {}
    virtual void imprimir(int indent = 0) const = 0;
};

class NodoContenedor : public Nodo {
protected:
    vector<Nodo*> hijos;

public:
    virtual ~NodoContenedor() {
        for (Nodo *h : hijos) delete h;
    }

    void agregarHijo(Nodo *nodo) {
        if (nodo) hijos.push_back(nodo);
    }

    void imprimirHijos(int indent) const {
        for (auto hijo : hijos) {
            hijo->imprimir(indent + 1);
        }
    }
};

class NodoPrograma : public NodoContenedor {
public:
    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Programa\n";
        imprimirHijos(indent);
    }
};

class NodoHoja : public Nodo {
protected:
    string etiqueta;
    string valor;

public:
    NodoHoja(const string &etiqueta_, const string &valor_)
        : etiqueta(etiqueta_), valor(valor_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << etiqueta << ": " << valor << "\n";
    }
};

class NodoDeclaracion : public Nodo {
public:
    string nombre;
    string tipo;
    int linea;

    NodoDeclaracion(const string &nombre_, const string &tipo_, int linea_)
        : nombre(nombre_), tipo(tipo_), linea(linea_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Declaracion " << nombre << " : " << tipo << " (linea " << linea << ")\n";
    }
};

class NodoAsignacion : public Nodo {
public:
    string nombre;
    string rhs;
    bool rhsEsVariable;
    int linea;

    NodoAsignacion(const string &nombre_, const string &rhs_, bool rhsEsVariable_, int linea_)
        : nombre(nombre_), rhs(rhs_), rhsEsVariable(rhsEsVariable_), linea(linea_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Asignacion " << nombre << " = " << rhs;
        if (rhsEsVariable) cout << " (variable)";
        cout << " (linea " << linea << ")\n";
    }
};

class NodoMostrar : public Nodo {
public:
    string valor;
    bool esVariable;
    int linea;

    NodoMostrar(const string &valor_, bool esVariable_, int linea_)
        : valor(valor_), esVariable(esVariable_), linea(linea_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Mostrar ";
        if (esVariable) cout << "var(" << valor << ")";
        else cout << "lit(" << valor << ")";
        cout << " (linea " << linea << ")\n";
    }
};

class NodoIngresar : public Nodo {
public:
    string nombre;
    int linea;

    NodoIngresar(const string &nombre_, int linea_)
        : nombre(nombre_), linea(linea_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Ingresar " << nombre << " (linea " << linea << ")\n";
    }
};

class NodoBloque : public NodoContenedor {
protected:
    string etiqueta;

public:
    NodoBloque(const string &etiqueta_) : etiqueta(etiqueta_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << etiqueta << "\n";
        imprimirHijos(indent);
    }
};

class NodoBis : public NodoBloque {
public:
    NodoBis() : NodoBloque("Bloque BIS") {}
};

class NodoRepetir : public NodoBloque {
public:
    int repeticiones;

    NodoRepetir() : NodoBloque("Bloque REPETIR"), repeticiones(0) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << etiqueta;
        if (repeticiones > 0) cout << " x" << repeticiones;
        cout << "\n";
        imprimirHijos(indent);
    }
};

class NodoSi : public NodoBloque {
public:
    string condicion;

    NodoSi() : NodoBloque("Bloque SI") {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << etiqueta << " (condicion: " << condicion << ")\n";
        imprimirHijos(indent);
    }
};

class NodoNota : public Nodo {
public:
    string nota;
    string alteracion;
    string duracion;
    int linea;

    NodoNota(const string &nota_, const string &alteracion_, const string &duracion_, int linea_)
        : nota(nota_), alteracion(alteracion_), duracion(duracion_), linea(linea_) {}

    void imprimir(int indent = 0) const override {
        imprimirIndent(indent);
        cout << "Nota " << nota;
        if (!alteracion.empty()) cout << alteracion;
        cout << " : " << duracion << " (linea " << linea << ")\n";
    }
};

#endif // AST_H
