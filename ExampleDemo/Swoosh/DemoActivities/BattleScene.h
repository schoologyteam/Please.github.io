#pragma once
#include <Swoosh\ActivityController.h>
#include <Swoosh\Game.h>
#include <Swoosh\Ease.h>
#include <Swoosh\EmbedGLSL.h>
#include <SFML\Graphics.hpp>
#include <SFML\Audio.hpp>

#include <Segues\WhiteWashFade.h>
#include <Segues\BlackWashFade.h>

#include "../ResourceManager.h"
#include "../Particle.h"
#include "../Pokemon.h"
#include "../Shaders.h"

#include "../ActionList.h"
#include "../BattleActions.h"

#include <iostream>

using namespace swoosh;
using namespace swoosh::intent;
using namespace swoosh::game;

class BattleScene : public Activity {
  friend class SignalRoundOver;
  friend class SignalCheckHP;
  friend class LeaveBattleScene;
  friend class SpawnNewPokemon;

private:
  ResourceManager& resources;

  sf::Sprite wildSprite;
  sf::Sprite playerSprite;
  sf::Sprite battleAreaSprite;
  sf::Sprite battlePadFGSprite;
  sf::Sprite battlePadBGSprite;
  sf::Sprite textboxSprite;
  sf::Sprite playerStatusSprite;
  sf::Sprite enemyStatusSprite;

  sf::Shader shader;
  sf::Shader statusShader;

  sf::Text   menuText;
  sf::Text   statusText;

  sf::Music battleMusic;
  sf::Sound sound;
  
  sf::View view;

  bool fadeMusic;
  bool preBattleSetup;
  bool doIntro;
  bool whiteFlash;  
  bool canInteract;
  bool isKeyDown;

  Timer waitTimer;
  ActionList actions;

  std::string output;

  int rowSelect;
  int colSelect;

  std::vector<particle> particles;
  std::vector<pokemon::monster>& playerMonsters;
  pokemon::monster wild;

public:

  BattleScene(ActivityController& controller, ResourceManager& resources, std::vector<pokemon::monster>& monsters) 
    : playerMonsters(monsters) , resources(resources), Activity(controller) {
    fadeMusic = preBattleSetup = canInteract = isKeyDown = doIntro = false;

    // Load sounds
    battleMusic.openFromFile(BATTLE_MUSIC_PATH);
    sound.setVolume(30);

    // Load shader
    shader.loadFromMemory(MONOTONE_SHADER, sf::Shader::Type::Fragment);
    shader.setUniform("texture", sf::Shader::CurrentTexture);

    statusShader.loadFromMemory(STATUS_BAR_SHADER, sf::Shader::Type::Fragment);
    statusShader.setUniform("texture", sf::Shader::CurrentTexture);

    // Refactoring: have one "loadPokemonData" for both wild and owned pokemon
    // Send pointers to the wild references and player references in the load function 
    // Have an additional argument to flag the facing direction
    wild = makeWildPokemon();
    loadPlayerPokemonData();

    // Load graphics
    battleAreaSprite   = sf::Sprite(*resources.battleAreaTexture);
    battlePadBGSprite  = sf::Sprite(*resources.battlePadBGTexture);
    battlePadFGSprite  = sf::Sprite(*resources.battlePadFGTexture);
    textboxSprite      = sf::Sprite(*resources.textboxTexture);
    playerStatusSprite = sf::Sprite(*resources.playerStatusTexture);
    enemyStatusSprite  = sf::Sprite(*resources.enemyStatusTexture);

    setOrigin(textboxSprite, 0.0, 1.0);
    textboxSprite.setScale(1.59f, 1.75f);

    // Load font
    menuText.setFont(*resources.menuFont);
    menuText.setScale(0.45, 0.45);
    menuText.setFillColor(sf::Color::Black);

    statusText.setFont(*resources.menuFont);
    statusText.setScale(0.2, 0.2);
    statusText.setFillColor(sf::Color::Black);

    // menu items
    colSelect = rowSelect = 0;
  }

  void loadPlayerPokemonData() {
    if (resources.playerTexture) delete resources.playerTexture;
    if (resources.playerRoarBuffer) delete resources.playerRoarBuffer;

    if (playerMonsters[0].name == "pikachu") {
      resources.playerRoarBuffer = resources.loadSound(PIKACHU_PATH[2]);
      resources.playerTexture = resources.loadTexture(PIKACHU_PATH[pokemon::facing::BACK]);
    } else if (playerMonsters[0].name == "charizard") {
      resources.playerRoarBuffer = resources.loadSound(CHARIZARD_PATH[2]);
      resources.playerTexture = resources.loadTexture(CHARIZARD_PATH[pokemon::facing::BACK]);
    } else if (playerMonsters[0].name == "roserade") {
      resources.playerRoarBuffer = resources.loadSound(ROSERADE_PATH[2]);
      resources.playerTexture = resources.loadTexture(ROSERADE_PATH[pokemon::facing::BACK]);
    } else if (playerMonsters[0].name == "onix") {
      resources.playerRoarBuffer = resources.loadSound(ONYX_PATH[2]);
      resources.playerTexture = resources.loadTexture(ONYX_PATH[pokemon::facing::BACK]);
    } else if (playerMonsters[0].name == "piplup") {
      resources.playerRoarBuffer = resources.loadSound(PIPLUP_PATH[2]);
      resources.playerTexture = resources.loadTexture(PIPLUP_PATH[pokemon::facing::BACK]);
    }

    playerSprite.setTexture(*resources.playerTexture, true);
    setOrigin(playerSprite, 0.5, 1.0);
  }

  void generateBattleActions(const pokemon::moves& playerchoice) {
    const pokemon::moves* monsterchoice = nullptr;

    if (wild.move1 == nullptr &&
        wild.move2 == nullptr &&
        wild.move3 == nullptr &&
        wild.move4 == nullptr) {
      monsterchoice = &pokemon::nomove;
    }

    // 2-step move
    if (wild.isFlying) monsterchoice = &pokemon::fly;

    while (monsterchoice == nullptr) {
      int randmove = rand() % 4;

      switch (randmove) {
      case 0:
        monsterchoice = wild.move1;
        break;
      case 1:
        monsterchoice = wild.move2;
        break;
      case 2:
        monsterchoice = wild.move3;
        break;
      case 3:
        monsterchoice = wild.move4;
        break;
      }
    }

    actions.clear();
    /* These two actions are non blocking
       make sure the pokemon have this to fall back to*/
    actions.add(new IdleAction(wildSprite));
    actions.add(new IdleAction(playerSprite));

    if (monsterchoice->speed > playerchoice.speed) {
      decideBattleOrder(&wild, monsterchoice, &wildSprite, &resources.wildRoarBuffer, &playerMonsters[0], &playerchoice, &playerSprite, &resources.playerRoarBuffer);
      decideBattleOrder(&playerMonsters[0], &playerchoice, &playerSprite, &resources.playerRoarBuffer, &wild, monsterchoice, &wildSprite, &resources.wildRoarBuffer);
    }
    else {
      decideBattleOrder(&playerMonsters[0], &playerchoice, &playerSprite, &resources.playerRoarBuffer, &wild, monsterchoice, &wildSprite, &resources.wildRoarBuffer);
      decideBattleOrder(&wild, monsterchoice, &wildSprite, &resources.wildRoarBuffer, &playerMonsters[0], &playerchoice, &playerSprite, &resources.playerRoarBuffer);
    }

    actions.add(new SignalRoundOver(this));
  }

  void decideBattleOrder(pokemon::monster* first, const pokemon::moves* firstchoice, sf::Sprite* firstSprite, sf::SoundBuffer** firstRoarBuffer,
    pokemon::monster* second, const pokemon::moves* secondchoice, sf::Sprite* secondSprite, sf::SoundBuffer** secondRoarBuffer) {
    pokemon::facing facing = pokemon::facing::BACK;

    if (first == &wild)
      facing = pokemon::facing::FRONT;

    ActionList* missedActionList = new ActionList();
    ActionList* defaultActionList = new ActionList();

    // The item below will set the isFlying attribute to false in the event the move that missed was "flying"
    (*missedActionList).add(new IdleAction(*firstSprite));
    (*missedActionList).add(new DisableFlyAction(*firstSprite, *first));
    (*missedActionList).add(new ChangeText(output, std::string(first->name) + " missed!"));
    (*missedActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

    if (first->isFlying) {
      (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " is flying!"));
      (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
      (*defaultActionList).add(new FlyAction(*firstSprite, *first, *resources.flyBuffer, sound));
      (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " attacked\nfrom above!"));
      (*defaultActionList).add(new TakeDamage(*second, firstchoice->damage, *resources.attackNormalBuffer, sound));
      (*defaultActionList).add(new SignalCheckHP(this));
      (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
    }
    else {
      if (firstchoice->name == "tackle") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used tackle!"));
        (*defaultActionList).add(new TackleAction(*firstSprite, facing));
        (*defaultActionList).add(new TakeDamage(*second, firstchoice->damage, *resources.attackNormalBuffer, sound));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

      }
      else if (firstchoice->name == "tail whip") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used tail whip!"));
        (*defaultActionList).add(new TailWhipAction(*firstSprite, *resources.tailWhipBuffer, sound));
        (*defaultActionList).add(new DefenseDownAction(*secondSprite, *resources.ddownTexture, *resources.statsFallBuffer, sound));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
        (*defaultActionList).add(new ChangeText(output, std::string(second->name) + "'s defense\nfell sharply!"));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

      }
      else if (firstchoice->name == "roar") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used roar!"));
        (*defaultActionList).add(new RoarAction(*firstSprite, firstRoarBuffer, sound));
        (*defaultActionList).add(new AttackUpAction(*firstSprite, *resources.aupTexture, *resources.statsRiseBuffer, sound));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + "'s attack\nrose!"));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

      }
      else if (firstchoice->name == "thunder") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used thunder!"));
        (*defaultActionList).add(new ThunderboltAction(*secondSprite, *resources.thunderBuffer, sound, *resources.boltTexture));
        (*defaultActionList).add(new TakeDamage(*second, firstchoice->damage, *resources.attackNormalBuffer, sound));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

      }
      else if (firstchoice->name == "fly") {
        (*defaultActionList).add(new FlyAction(*firstSprite, *first, *resources.flyBuffer, sound));
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used fly!"));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " flew\nup into the sky!"));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));

      }
      else if (firstchoice->name == "flamethrower") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " used\nflamethrower!"));
        (*defaultActionList).add(new FlamethrowerAction(*this, *firstSprite, facing, *resources.flameBuffer, sound, *secondSprite, *resources.fireTexture));
        (*defaultActionList).add(new TakeDamage(*second, firstchoice->damage, *resources.attackNormalBuffer, sound));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
      }
      else if (firstchoice->name == "nomove") {
        (*defaultActionList).add(new ChangeText(output, std::string(first->name) + " is struggling!"));
        (*defaultActionList).add(new TakeDamage(*second, firstchoice->damage, *resources.attackNormalBuffer, sound));
        (*defaultActionList).add(new TakeDamage(*first, firstchoice->damage, *resources.attackNormalBuffer, sound));
        (*defaultActionList).add(new SignalCheckHP(this));
        (*defaultActionList).add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
      }
    }

    // Our attacks miss unless its a flying move- we can go up in the sky and try to hit next turn
    // Check that they are not also flying while coming down
    // Roar is an exception as it affects ourself 
    auto condition = [second, first, firstchoice]() {
      return (second->isFlying
        && first->isFlying
        && &(*firstchoice) == &pokemon::fly)
        || (second->isFlying && &(*firstchoice) != &pokemon::roar && &(*firstchoice) != &pokemon::fly)
        ;  
    };

    actions.add(new ConditionalAction(condition, missedActionList, defaultActionList));
  }

  pokemon::monster makeWildPokemon() {
    pokemon::monster wild;
    
    int select = rand() % 6;

    if (resources.wildTexture) delete resources.wildTexture;
    if (resources.wildRoarBuffer) delete resources.wildRoarBuffer;

    switch (select) {
    case 0:
      wild = pokemon::monster(pokemon::pidgey);
      resources.wildTexture = resources.loadTexture(PIDGEY_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(PIDGEY_PATH[2]);
      wild.level = 5 + rand() % 8;
      wild.xp = 35; // base worth 35 xp
      break;
    case 1:
      wild = pokemon::monster(pokemon::clefairy);
      resources.wildTexture = resources.loadTexture(CLEFAIRY_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(CLEFAIRY_PATH[2]);
      wild.level = 5 + rand() % 4;
      wild.xp = 40; // base worth 40 xp

      break;
    case 2:
      wild = pokemon::monster(pokemon::geodude);
      resources.wildTexture = resources.loadTexture(GEODUDE_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(GEODUDE_PATH[2]);
      wild.level = 5 + rand() % 4;
      wild.xp = 53; // base worth 53 xp

      break;
    case 3:
      wild = pokemon::monster(pokemon::ponyta);
      resources.wildTexture = resources.loadTexture(PONYTA_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(PONYTA_PATH[2]);
      wild.level = 5 + rand() % 7;
      wild.xp = 51; // base worth 51 xp

      break;
    case 4:
      wild = pokemon::monster(pokemon::cubone);
      resources.wildTexture = resources.loadTexture(CUBONE_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(CUBONE_PATH[2]);
      wild.level = 5 + rand() % 4;
      wild.xp = 40; // base worth 40 xp

      break;
    case 5:
      wild = pokemon::monster(pokemon::oddish);
      resources.wildTexture = resources.loadTexture(ODISH_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(ODISH_PATH[2]);
      wild.level = 5 + rand() % 1;
      wild.xp = 35; // base worth 35 xp

      break;
    case 6:
      wild = pokemon::monster(pokemon::pikachu);
      resources.wildTexture = resources.loadTexture(PIKACHU_PATH[pokemon::facing::FRONT]);
      wildSprite = sf::Sprite(*resources.wildTexture);
      resources.wildRoarBuffer = resources.loadSound(PIKACHU_PATH[2]);
      wild.level = 5 + rand() % 8;
      wild.xp = 39; // base worth 39 xp

      break;
    }

    setOrigin(wildSprite, 0.5, 1.0);
    return wild;
  }

  virtual void onStart() {
    std::cout << "BattleScene OnStart called" << std::endl;

    // Start timer 
    waitTimer.start();
  }

  void updateParticles(double elapsed) {
    int i = 0;
    for (auto& p : particles) {
      p.speed +=  sf::Vector2f(p.acceleration.x * elapsed, p.acceleration.y * elapsed);
      p.pos += sf::Vector2f(p.speed.x * p.friction.x * elapsed , p.speed.y * p.friction.y * elapsed );

      p.sprite.setPosition(p.pos);

      double alpha = 0;

      if (p.sprite.getTexture() == resources.fireTexture)
        alpha = 1.0;

      p.sprite.setScale(2.0*(alpha - (p.life / p.lifetime)), 2.0*(alpha - (p.life / p.lifetime)));
      p.sprite.setColor(sf::Color(p.sprite.getColor().r, p.sprite.getColor().g, p.sprite.getColor().b, 255 * p.life / p.lifetime));
      p.sprite.setRotation(p.sprite.getRotation() + (elapsed * 20));
      p.life -= elapsed;

      if (p.life <= 0) {
        particles.erase(particles.begin() + i);
        continue;
      }

      i++;
    }
  }

  public:

  // used as shorthand notation for pokeball
  void spawnPokeballParticles(sf::Texture* texture, sf::Vector2f position, int numPerFrame=100) {
    for (int i = numPerFrame; i > 0; i--) {
      int randNegative = rand() % 2 == 0 ? -1 : 1;
      int randSpeedX = rand() % 220;
      randSpeedX *= randNegative;
      int randSpeedY = rand() % 320;

      particle p;
      p.sprite = sf::Sprite(*texture);
      setOrigin(p.sprite, 0.5, 0.5);

      p.pos = position;
      p.speed = sf::Vector2f(randSpeedX, -randSpeedY);
      p.friction = sf::Vector2f(0.46f, 0.46f);
      p.life = 1;
      p.lifetime = 1;
      p.sprite.setPosition(p.pos);

      particles.push_back(p);
    }

    updateParticles(0); // set sprite positions for drawing
  }

  void spawnParticles(sf::Texture* texture, sf::Vector2f position, sf::Vector2f speed, sf::Vector2f friction, sf::Vector2f accel, int numPerFrame) {
    for (int i = numPerFrame; i > 0; i--) {
      particle p;
      p.sprite = sf::Sprite(*texture);
      setOrigin(p.sprite, 0.5, 0.5);
      p.pos = position;
      p.speed = speed;
      p.acceleration = accel;
      p.friction = friction;
      p.life = 1;
      p.lifetime = 1;
      p.sprite.setPosition(p.pos);

      particles.push_back(p);
    }

    updateParticles(0); // set sprite positions for drawing
  }

  private:
  virtual void onUpdate(double elapsed) {
    // Speed up the battle when holding down this key
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space))
      elapsed *= 2.5;

    updateParticles(elapsed);

    if (fadeMusic) {
      battleMusic.setVolume(battleMusic.getVolume() * 0.90); // quieter
    }

    // 2 seconds at most for the intro
    if (waitTimer.getElapsed().asSeconds() > 2) {
      if (!preBattleSetup) {
        // Before the round begins, we clear the action list
        // And make the player bob
        actions.add(new ClearPreviousActions());
        actions.add(new IdleAction(wildSprite));
        actions.add(new BobAction(playerSprite));

        // Only show this one time at the start of the scene
        if (!doIntro) {
          actions.add(new ChangeText(output, "A wild " + std::string(wild.name) + " appeard!"));
          actions.add(new RoarAction(wildSprite, &resources.wildRoarBuffer, sound));
          actions.add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
          actions.add(new SignalRoundOver(this)); // Prepare for keyboard interaction
          doIntro = true;
        }

        preBattleSetup = true;
      }
      actions.update(elapsed);
    }
    else {
      auto windowSize = getView().getSize();
      double alpha = ease::linear(waitTimer.getElapsed().asSeconds(), 2.0f, 1.0f);

      setOrigin(battlePadBGSprite, 0.5, 0.5);
      setOrigin(battlePadFGSprite, 0.35, 1.0);

      // All this does is divide the screen into 4 parts and places the pokemon into one of the corners.
      // I offset the position by accounting for the origin placement so that the pokemon appear offscreen
      // but when alpha reaches = 1, then they should be at those corners.
      // This could be reduced and look easier to follow but am lazy
      float backx = ((((float)windowSize.x / 4.0f)*3.0f + wildSprite.getTexture()->getSize().x*2.0f)*alpha) - (wildSprite.getTexture()->getSize().x*2.0f);
      float frontx = (float)windowSize.x*(1.0 - alpha) + ((float)windowSize.x / 4.0f);
      wildSprite.setPosition(backx, (float)windowSize.y / 2.0f);
      playerSprite.setPosition(frontx, ((float)windowSize.y / 4.0f) * 3.0f);

      battlePadBGSprite.setPosition(backx, (float)windowSize.y / 2.0f);
      battlePadFGSprite.setPosition(frontx, ((float)windowSize.y / 4.0f) * 3.0f);
    }

    if (preBattleSetup && sf::Keyboard::isKeyPressed(sf::Keyboard::BackSpace)) {
      using intent::segue;
      getController().queuePop<segue<BlackWashFade>>();
    }

    if (canInteract) {
      output = ""; // clear our output text otherwise it shows up briefly

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
        rowSelect--;
      }
      else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
        rowSelect++;
      }
      else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
        colSelect--;
      }
      else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
        colSelect++;
      }

      rowSelect = std::max(0, rowSelect);
      rowSelect = std::min(rowSelect, 1);
      colSelect = std::max(0, colSelect);
      colSelect = std::min(colSelect, 1);

      // Clear for the next round
      //actions.clear();

      const pokemon::moves* choice = &pokemon::nomove;

      // Flying pokemon take 2 turns
      if (playerMonsters[0].isFlying) {
        isKeyDown = false; 
        canInteract = false;
        choice = &pokemon::fly;

        actions.add(new ChangeText(output, std::string() + playerMonsters[0].name + " is\nstill in the sky!"));
        actions.add(new WaitForButtonPressAction(sf::Keyboard::Enter, *resources.selectBuffer, sound));
        generateBattleActions(*choice);
        return;
      }
      else {
        if (colSelect == 0 && rowSelect == 0) {
          choice = playerMonsters[0].move1;
        }

        if (colSelect == 1 && rowSelect == 0) {
          choice = playerMonsters[0].move2;
        }

        if (colSelect == 0 && rowSelect == 1) {
          choice = playerMonsters[0].move3;
        }

        if (colSelect == 1 && rowSelect == 1) {
          choice = playerMonsters[0].move4;
        }
      }

      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Enter) && !isKeyDown) {
        if (choice != nullptr) {
          isKeyDown = true;
          sound.setBuffer(*resources.selectBuffer);
          sound.play();
        } // else play a buzzer sound
        else {
          isKeyDown = true;
          sound.setBuffer(*resources.buzzerBuffer);
          sound.play();
        }
      } // force player to release key to register as a press only on valid entries
      else if (canInteract && !sf::Keyboard::isKeyPressed(sf::Keyboard::Enter) && isKeyDown) {
        isKeyDown = false; 

        // We made a valid selection
        if (choice != nullptr) {
          canInteract = false;
          generateBattleActions(*choice);
        }
      }
    }

    if (waitTimer.getElapsed().asSeconds() > 2) {
      shader.setUniform("amount", 0.0f);
    }
    else {
      shader.setUniform("amount", 1.0f);
    }

    textboxSprite.setPosition(0, getView().getSize().y);
  }

  virtual void onLeave() {
    std::cout << "BattleScene OnLeave called" << std::endl;
    fadeMusic = true;
  }

  virtual void onExit() {
    std::cout << "BattleScene OnExit called" << std::endl;

    if (fadeMusic) {
      playerMonsters[0].isFlying = false; // reset in the event we left early (debug key press)
      battleMusic.stop();
    }
  }

  virtual void onEnter() {
    std::cout << "BattleScene OnEnter called" << std::endl;
    battleMusic.play();
  }


  virtual void onResume() {
    std::cout << "BattleScene OnResume called" << std::endl;
  }

  virtual void onDraw(sf::RenderTexture& surface) {
    setView(sf::View(sf::FloatRect(0.0f, 0.0f, 400.0f, 300.0f)));

    sf::RenderStates states;
    states.shader = &shader;

    if (waitTimer.getElapsed().asSeconds() > 2) {
      surface.draw(battleAreaSprite);
      surface.draw(battlePadBGSprite);
      surface.draw(battlePadFGSprite);
      actions.draw(surface);

      for (auto& p : particles) {
        surface.draw(p.sprite);
      }

      surface.draw(textboxSprite);

      // Position the ui
      setOrigin(playerStatusSprite, 1.0, 1.0); // bottom-right corner
      playerStatusSprite.setPosition(getView().getSize().x, textboxSprite.getPosition().y-textboxSprite.getGlobalBounds().height);
      setOrigin(enemyStatusSprite, 0.0, 0.0); // top-left corner
      enemyStatusSprite.setPosition(0, 0);

      // draw the ui
      states.shader = &statusShader;

      // The ui shader takes in hp and xp from 0.0 to 1.0
      float hp = (float)playerMonsters[0].hp / (float)playerMonsters[0].maxhp;
      statusShader.setUniform("hp", hp);
      statusShader.setUniform("xp", (float)playerMonsters[0].xp / 100.0f);
      surface.draw(playerStatusSprite, states);
      statusText.setString(std::to_string(std::max(0, playerMonsters[0].hp)));
      statusText.setPosition(playerStatusSprite.getPosition().x - 58, playerStatusSprite.getPosition().y - 15);
      surface.draw(statusText);
      statusText.setString(std::to_string(playerMonsters[0].maxhp));
      statusText.setPosition(playerStatusSprite.getPosition().x - 33, playerStatusSprite.getPosition().y - 15);
      surface.draw(statusText);
      statusText.setString(std::to_string(playerMonsters[0].level));
      statusText.setPosition(playerStatusSprite.getPosition().x - 21, playerStatusSprite.getPosition().y - 35);
      surface.draw(statusText);

      statusShader.setUniform("hp", (float)wild.hp / (float)wild.maxhp);
      surface.draw(enemyStatusSprite, states);
      statusText.setString(std::to_string(wild.level)); // every wild pokemon is level 5 for demo
      statusText.setPosition(enemyStatusSprite.getPosition().x + 85, enemyStatusSprite.getPosition().y + 11);
      surface.draw(statusText);

      double columnRight = getView().getSize().x / 2.0;
      double columnLeft = 12;
      double rowTop = 10 + getView().getSize().y / 4.0 * 3.0;
      double rowBot = 10 + getView().getSize().y / 4.0 * 3.5;

      // Draw a menu of the current pokemon's abilities
      if (canInteract) {
        menuText.setString("�");

        // Draw the karat
        if (colSelect == 0 && rowSelect == 0) {
          menuText.setPosition(columnLeft, rowTop);
        }

        if (colSelect == 1 && rowSelect == 0) {
          menuText.setPosition(columnRight, rowTop);
        }

        if (colSelect == 0 && rowSelect == 1) {
          menuText.setPosition(columnLeft, rowBot);
        }

        if (colSelect == 1 && rowSelect == 1) {
          menuText.setPosition(columnRight, rowBot);
        }

        surface.draw(menuText);

        // Now draw the title of the moves
        menuText.setPosition(columnLeft + 10, rowTop);

        if(playerMonsters[0].move1 == nullptr) {
          menuText.setString("-");
        }
        else {
          menuText.setString(playerMonsters[0].move1->name);
        }

        surface.draw(menuText);

        menuText.setPosition(10 + columnRight, rowTop);
        if (playerMonsters[0].move2 == nullptr) {
          menuText.setString("-");
        }
        else {
          menuText.setString(playerMonsters[0].move2->name);
        }

        surface.draw(menuText);

        menuText.setPosition(10 + columnLeft, rowBot);
        if (playerMonsters[0].move3 == nullptr) {
          menuText.setString("-");
        }
        else {
          menuText.setString(playerMonsters[0].move3->name);
        }

        surface.draw(menuText);

        menuText.setPosition(10 + columnRight, rowBot);
        if (playerMonsters[0].move4 == nullptr) {
          menuText.setString("-");
        }
        else {
          menuText.setString(playerMonsters[0].move4->name);
        }

        surface.draw(menuText);

      }
      else {
        menuText.setString(output);
        menuText.setPosition(2 + columnLeft, rowTop);
        surface.draw(menuText);
      }
    }
    else {
      // Otherwise draw just the pokemon in greyscale for the intro
      surface.draw(battlePadBGSprite, states);
      surface.draw(battlePadFGSprite, states);
      surface.draw(wildSprite, states);
      surface.draw(playerSprite, states);
    }
  }

  virtual void onEnd() {
    std::cout << "BattleScene OnEnd called" << std::endl;
  }

  virtual ~BattleScene() {
  }
};