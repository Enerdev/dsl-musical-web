/***
 ============================================================
  DSL MUSICAL -- COMPILADOR (VERSION WEB)
  Analisis Lexico, Sintactico y Semantico
  Archivo: main_web.cpp
  Descripcion: Punto de entrada para despliegue en la nube.
               A diferencia de main.cpp, este NO lee un
               archivo del disco ni depende de argv: lee el
               codigo fuente completo desde stdin y no
               bloquea con pausas de consola (getline al
               final). Pensado para ser invocado por un
               backend (por ejemplo en Railway) que reciba el
               codigo .mus por POST desde el frontend y se lo
               pase por stdin a este binario ya compilado,
               devolviendo stdout como la respuesta.
 ============================================================
***/

#include <iostream>
#include <sstream>
#include <string>
#include "analizador_lexico.h"
#include "analizador_sintactico.h"

using namespace std;

int main() {
    // -- Leer TODO el stdin como el codigo fuente del .mus --
    // (el backend hace algo como: echo "$codigo" | ./compilador)
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    string src = buffer.str();

    if (src.empty()) {
        cout << "\n  [ERROR] No se recibio codigo fuente (stdin vacio).\n";
        return 1;
    }

    cout << "\n";
    cout << "======================================================\n";
    cout << "  DSL MUSICAL -- COMPILADOR (WEB)\n";
    cout << "  Analisis Lexico, Sintactico y Semantico\n";
    cout << "======================================================\n";

    AnalizadorLexico *lexer = new AnalizadorLexico(src.c_str());

    // -- Mostrar el codigo fuente recibido --
    lexer->MostrarFuente();

    // -- Analisis Lexico --
    if (lexer->Lexico()) {
        // -- Analisis Sintactico (el Semantico va integrado dentro) --
        AnalizadorSintactico *parser = new AnalizadorSintactico(lexer);
        parser->Sintactico();
        delete parser;
    }

    delete lexer;
    return 0;
}
