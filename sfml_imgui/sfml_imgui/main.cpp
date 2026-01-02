#include "pch.h"

int main()
{
	sf::RenderWindow window(sf::VideoMode({ 800, 800 }), "IUiBG", sf::Style::Titlebar | sf::Style::Close);
	sf::Clock deltaClock;
	sf::RectangleShape rectangle;
	sf::CircleShape circle;
	sf::Vector2 circle_position(300.0f, 200.0f);

	window.setFramerateLimit(60);
	std::ignore = ImGui::SFML::Init(window);

	rectangle.setSize(sf::Vector2f(200.0f, 100.0f));
	rectangle.setFillColor(sf::Color(128, 128, 128));
	rectangle.setOutlineColor(sf::Color::White);
	rectangle.setOutlineThickness(5.0f);
	rectangle.setPosition(sf::Vector2f(250.0f, 250.0f));

	circle.setRadius(20.0f);
	circle.setFillColor(sf::Color::Transparent);
	circle.setOutlineColor(sf::Color::Red);
	circle.setOutlineThickness(5.0f);

	while (window.isOpen())
	{
		while (const std::optional event = window.pollEvent())
		{
			ImGui::SFML::ProcessEvent(window, *event);
			if (event->is<sf::Event::Closed>()) window.close();
		}
		ImGui::SFML::Update(window, deltaClock.restart());
		ImGui::Begin("Test window");
		ImGui::SliderFloat2("x,y", &circle_position.x, 10.0f, 750.0f);
		ImGui::Separator();
		if (ImGui::Button("Ping")) std::cout << "Wcisnieto guzik ,,Ping''." << std::endl;
		ImGui::End();

		circle.setPosition(circle_position);

		window.clear(sf::Color::Black);

		window.draw(rectangle);
		window.draw(circle);
		ImGui::SFML::Render(window);

		window.display();
	}
	return 0;
}