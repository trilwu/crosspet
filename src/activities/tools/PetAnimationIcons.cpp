#include "PetAnimationIcons.h"

void drawPetActionIcon(GfxRenderer& renderer, PetAnimIcon icon, int x, int y) {
  switch (icon) {
    case PetAnimIcon::HEART: {
      renderer.fillRect(x + 2, y,     4, 2);
      renderer.fillRect(x + 10, y,    4, 2);
      renderer.fillRect(x, y + 2,     16, 2);
      renderer.fillRect(x + 1, y + 4, 14, 2);
      renderer.fillRect(x + 2, y + 6, 12, 2);
      renderer.fillRect(x + 3, y + 8, 10, 2);
      renderer.fillRect(x + 4, y + 10, 8, 2);
      renderer.fillRect(x + 5, y + 12, 6, 2);
      renderer.fillRect(x + 6, y + 14, 4, 2);
      break;
    }
    case PetAnimIcon::FOOD: {
      renderer.fillRect(x + 2, y + 4, 12, 2);
      renderer.fillRect(x + 3, y + 6, 10, 4);
      renderer.fillRect(x + 4, y + 10, 8, 2);
      renderer.fillRect(x + 5, y + 12, 6, 2);
      renderer.fillRect(x + 5, y,     2, 2);
      renderer.fillRect(x + 9, y + 1, 2, 2);
      break;
    }
    case PetAnimIcon::SNACK: {
      renderer.fillRect(x + 4, y + 2, 8, 2);
      renderer.fillRect(x + 2, y + 4, 12, 8);
      renderer.fillRect(x + 4, y + 12, 8, 2);
      renderer.fillRect(x + 5, y + 6, 2, 2, false);
      renderer.fillRect(x + 9, y + 8, 2, 2, false);
      break;
    }
    case PetAnimIcon::MEDICINE: {
      renderer.fillRect(x + 6, y + 2, 4, 12);
      renderer.fillRect(x + 2, y + 6, 12, 4);
      break;
    }
    case PetAnimIcon::EXERCISE: {
      renderer.fillRect(x + 8, y,     4, 2);
      renderer.fillRect(x + 6, y + 2, 4, 2);
      renderer.fillRect(x + 4, y + 4, 4, 2);
      renderer.fillRect(x + 2, y + 6, 10, 2);
      renderer.fillRect(x + 8, y + 8, 4, 2);
      renderer.fillRect(x + 6, y + 10, 4, 2);
      renderer.fillRect(x + 4, y + 12, 4, 2);
      break;
    }
    case PetAnimIcon::CLEAN: {
      renderer.fillRect(x + 3, y + 2, 2, 2);
      renderer.fillRect(x + 2, y + 4, 4, 4);
      renderer.fillRect(x + 10, y + 6, 2, 2);
      renderer.fillRect(x + 9, y + 8, 4, 4);
      break;
    }
    case PetAnimIcon::SCOLD: {
      renderer.fillRect(x + 6, y + 2, 4, 8);
      renderer.fillRect(x + 6, y + 12, 4, 3);
      break;
    }
    case PetAnimIcon::SLEEP: {
      renderer.fillRect(x + 2, y + 2, 6, 2);
      renderer.fillRect(x + 6, y + 4, 2, 2);
      renderer.fillRect(x + 4, y + 6, 2, 2);
      renderer.fillRect(x + 2, y + 8, 6, 2);
      renderer.fillRect(x + 8, y + 6, 4, 2);
      renderer.fillRect(x + 10, y + 8, 2, 2);
      renderer.fillRect(x + 8, y + 10, 4, 2);
      break;
    }
    default: break;
  }
}
