// Swoosh.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <Swoosh/ActivityController.h>
#include "DemoActivities/OverworldScene.h"
#include "DemoActivities/MainMenuScene.h"
#include "ResourceManager.h"

using namespace swoosh;

void loadResources(ResourceManager* resources) {
  resources->load();
}

int main()
{
  sf::RenderWindow window(sf::VideoMode(800, 600), "HeartGold Swoosh");
  window.setFramerateLimit(60); // call it once, after creating the window
  window.setMouseCursorVisible(false);

  ResourceManager resources;

  sf::Texture* pokeballTexture = resources.loadTexture(POKEBALL_ICON_PATH);
  sf::Sprite pokeball(*pokeballTexture);
  setOrigin(pokeball, 0.5, 0.5);
  pokeball.setPosition(window.getSize().x / 2.0, window.getSize().y / 2.0);

  sf::Thread loadThread(loadResources, &resources);
  loadThread.launch();

  sf::RenderTexture snapshot;
  snapshot.create((unsigned int)window.getSize().x, (unsigned int)window.getSize().y);

  float elapsed = 0.0f;
  sf::Clock clock;

  srand((unsigned int)time(0));

  // Preloader loop
  while (window.isOpen() && !resources.isLoaded())
  {
    clock.restart();

    // check all the window's events that were triggered since the last iteration of the loop
    sf::Event event;
    while (window.pollEvent(event))
    {
      // "close requested" event: we close the window
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    window.clear();
    // Rorate the pokeball with a speed based on loading progress
    pokeball.setRotation(90.0*elapsed*resources.getProgress() + pokeball.getRotation());
    window.draw(pokeball);
    window.display();

    elapsed = static_cast<float>(clock.getElapsedTime().asSeconds());
  }

  delete pokeballTexture;

  /***********************************************
  Now that the graphics are ready, load the scenes
  ***********************************************/

  ActivityController app(window);
  app.push<OverworldScene>(resources);
  app.draw(snapshot); // draw to another surface
  snapshot.display(); // ready buffer for copying
  sf::Texture overworldSnapshot(snapshot.getTexture());
  app.push<MainMenuScene>(resources, overworldSnapshot); // Pass into next scene

  // Main loop
  while (window.isOpen())
  {
    clock.restart();

    // check all the window's events that were triggered since the last iteration of the loop
    sf::Event event;
    while (window.pollEvent(event))
    {
      // "close requested" event: we close the window
      if (event.type == sf::Event::Closed) {
        window.close();
      }
    }

    window.clear();
    app.update(elapsed);
    app.draw();
    window.display();

    elapsed = static_cast<float>(clock.getElapsedTime().asSeconds());
  }

  return 0;
}