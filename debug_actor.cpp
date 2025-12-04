#include "mermaid.h"
#include <iostream>

int main() {
    std::string source = R"(sequenceDiagram
  actor User
  participant Server

  User->>Server: Login
  Server->>User: Token)";

    std::string svg = mermaid::render_to_svg(source);
    std::cout << "SVG output:\n" << svg << std::endl;
    
    // Check if actor is rendered as stick figure
    if (svg.find("actor-man") != std::string::npos) {
        std::cout << "SUCCESS: Actor rendered as stick figure (actor-man found)" << std::endl;
    } else {
        std::cout << "FAILURE: Actor not rendered as stick figure (actor-man not found)" << std::endl;
        std::cout << "Looking for 'actor-man' in SVG..." << std::endl;
    }
    
    return 0;
}
