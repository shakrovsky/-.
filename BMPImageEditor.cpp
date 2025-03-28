#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <string>

class BMPImageEditor
{
private:
    /*  Отключаю выравнивание данных структуры в памяти.
        Данная манипуляция нужна для корректного считывания байтов из файла в структуры.  */
    #pragma pack(push, 1)

    // Описание структуры *заголовка* BMP-файла.
    struct BMPFileHeader
    {
        uint16_t type_of_file;              //  2 байта: Тип файла.

        uint32_t size_of_file;              /*  4 байта: Размер всего файла в байтах
                                                (включая заголовки и данные о пикселях).  */

        uint16_t reserved_1;                //  2 байта: Зарезервированно.

        uint16_t reserved_2;                //  2 байта: Зарезервированно.

        uint32_t offset_to_pixel_data;      /*  4 байта: Смещение (в байтах) от начала файла до начала данных о пикселях.
                                                Обычно, смещение = размер заголовка файла + размер информационного блока.  */
    };

    // Описание структуры *информационного блока* BMP-файла.
    struct BMPFileInfoBlock
    {
        uint32_t size_of_info_block;        //  4 байта: Размер (в байтах) информационного блока.

        uint32_t width;                     //  4 байта: Ширина изображения в пикселях.

        uint32_t height;                    //  4 байта: Высота изображения в пикселях.

        uint16_t count_of_planes;           /*  2 байта: Количество цветовых плоскостей
                                                (для 2D изображений количество плоскостей всегда = 1).  */

        uint16_t color_depth_in_bits;       //  2 байта: Глубина цвета (кол-во бит на один пиксель).

        uint32_t type_of_compression;       //  4 байта: Тип сжатия (0 -> без сжатия).

        uint32_t size_of_image;             //  4 байта: Размер изображения в байтах (может быть 0 для несжатых).

        int32_t  horizontal_resolution;     //  4 байта: Горизонтальное разрешение (пикселей на метр).

        int32_t  vertical_resolution;       //  4 байта: Вертикальное разрешение (пикселей на метр).

        uint32_t count_of_colors;           //  4 байта: Количество используемых цветов в палитре.

        uint32_t count_of_important_colors; //  4 байта: Количество важных цветов (0 - все важны)
    };

    #pragma pack(pop)

    /*  Создаю объекты вышереализованных структур.
        Они понадобятся для обращения к полям (которые содержат нужную информацию).  */
    BMPFileHeader file_header;
    BMPFileInfoBlock info_block;

    /*  Данная матрица содержит цвета каждого пикселя исходного изображения 
        в правильном порядке. Позволяет смоделировать изображение в консоли.  */
    std::vector<std::vector<uint32_t>> pixels;

    /*  Завожу данные переменные для формирования изображения в консоли.
        white соответствует белому пикселю, black - черному пикселю, а 
        unknown_color - пикселю с неизвестным мне цветом.  */
    std::string white =         "..";
    std::string black =         "$$";
    std::string unknown_color = "??";

    // Флаг, обозначающий, открыл ли объект данного класса некоторый входной файл.
    bool fileWasRead = false;

public:
    BMPImageEditor() = default;

    // Метод, позволяющий считать все данные из входного файла.
    void read(const std::string& file_path) 
    {
        // 1. Создаю входной поток для чтения входного файла.
        std::ifstream inp_file(file_path);

        // 1.1 Если не смог открыть файл - выбрасываю исключение с соответствующим сообщением.
        if (!inp_file) {
            throw std::runtime_error("Error! It's not possible to open the file on the path: \"" + file_path + "\".");
        }

        /* 2.   Создаю функтор (с помощью лямбда-функции) для удобной проверки на возникновение ошибок при чтении файла.
                При необходимости функтор выбрасывает исключение с соответствующим сообщением.  */
        auto check_error = [&]() { if (inp_file.fail()) throw std::runtime_error("Oops! An error occurred while reading the file."); };

        /* 3.   Считываю информацию из двух основных заголовков и заполняю поля объектов
                моих структур соответствующей информацией. После чтения каждого блока проверяю наличие ошибок.   */
        inp_file.read(reinterpret_cast<char*>(&file_header), sizeof(BMPFileHeader));
        check_error();

        inp_file.read(reinterpret_cast<char*>(&info_block), sizeof(BMPFileInfoBlock));
        check_error();

        /* 3.1  После получения информации о входном файле, обрабатываю случаи, 
                которые могут привести к *непредвиденному* поведению программы.  */
        if (file_header.type_of_file != 0x4D42) {
            throw std::runtime_error("Error! The specified file is not of the BMP type.");
        }

        if (info_block.color_depth_in_bits != 24) {
            throw std::runtime_error("Error! This class only works with images with a color depth of 24 bits.");
        }

        /* 4.   Внутри файла перемещаюсь к началу данных о пикселях.
                На самом деле, в моем случае это не обязательно, так как позиция чтения 
                и так находится перед информацией о пикселях, но все же стоит перестраховаться.   */
        inp_file.seekg(file_header.offset_to_pixel_data, std::ios::beg);

        /* 5.   Так как данные о пикселях в файле обычно располагаются с учетом
                выравнивания, то я рассчитываю реальный размер строки, учитывая вышеупомянутый момент.   */
        int bytes_per_pixel = info_block.color_depth_in_bits / 8;
        int row_size = (info_block.width * bytes_per_pixel + 3) & (~3);

        // 6. Выделяю необходимую память для матрицы pixels.
        pixels.resize(info_block.height, std::vector<uint32_t>(info_block.width));

        // 7. Начинаю считывать информацию о пикселях:
        for (int y = 0; y < info_block.height; ++y)
        {
            /* 7.1  Определяю правильный индекс строки в изображении
                    (иначе изображение выводится вверх ногами).  */
            int row_index = info_block.height - y - 1;

            for (int x = 0; x < info_block.width; ++x)
            {
                /*  Напоминаю, каждый пиксель представлен 3 байтами:
                    1-й байт -> синий, 2-й байт -> зеленый, 3-й байт -> красный.  */
                uint8_t blue = 0, green = 0, red = 0;

                inp_file.read(reinterpret_cast<char*>(&blue), sizeof(uint8_t));
                inp_file.read(reinterpret_cast<char*>(&green), sizeof(uint8_t));
                inp_file.read(reinterpret_cast<char*>(&red), sizeof(uint8_t));

                //  nothing      blue        green        red
                // 0000_0000 ' 0000_0000 ' 0000_0000 ' 0000_0000
                uint32_t color = (blue << 16) | (green << 8) | red;

                pixels[row_index][x] = color;
            }

            /* 7.2  Вычисляю количество байт выравнивания для
                    данной строки и сдвигаю позицию чтения при необходимости.  */
            int padding = row_size - (info_block.width * bytes_per_pixel);
            if (padding > 0) { inp_file.seekg(padding, std::ios::cur); }
        }

        // 8. Закрываю поток чтения входного файла и меняю флаг fileWasRead на соответствующее значение.
        inp_file.close();
        fileWasRead = true;
    }

    /*  Метод, позволяющий нарисовать крест на изображении. Пользователь может выбрать, 
        каким цветом ему нарисовать крест -> для этого ему достаточно ввести BGR-последовательность
        (по умолчанию - крест рисуется черным цветом).  */
    void drawCross(uint8_t blue = 0, uint8_t green = 0, uint8_t red = 0)
    {        
        /* 1.   Если на момент вызова данной функции пользователь 
                еще не считал данные из файла - выбрасываю исключение с соответствующим сообщением.   */
        if (!fileWasRead) {
            throw std::runtime_error("Error! First you need to read the data from the file.");
        }

        // 2. Интерпретирую входные цвета как единое 4х-байтовое число.
        uint32_t color = (blue << 16) | (green << 8) | red;

        // 3. Рисую крест, который проходит по главным диагоналям изображения.
        for (int i = 0; i < std::min(info_block.height, info_block.width); ++i)
        {
            pixels[i][i] = color;
            pixels[i][info_block.width - i - 1] = color;
        }
    }

    // Метод, позволяющий сохранить изображение в некоторый файл.
    void save(const std::string& file_path)
    {
        // 1. Создаю выходной поток для записи в некоторый файл.
        std::ofstream out_file(file_path, std::ios::out | std::ios::trunc);

        // 2. Выравниваю строку по 4 байта.
        int32_t row_stride = ((info_block.width * info_block.color_depth_in_bits + 31) / 32) * 4;

        // 3. Записываю в файл информацию о первых двух блоках.
        out_file.write(reinterpret_cast<char*>(&file_header), sizeof(BMPFileHeader));
        out_file.write(reinterpret_cast<char*>(&info_block), sizeof(BMPFileInfoBlock));

        // 4. В BMP-файле строки располагаются в обратном порядке (снизу вверх).
        for (int y = info_block.height - 1; y >= 0; --y) 
        {
            // 4.1 Создаю вектор, который будет содержать все байты некоторой строки.
            std::vector<uint8_t> row_data;

            for (int x = 0; x < info_block.width; ++x) 
            {
                /* 4.2  Обрабатываю каждый пиксель, и в зависимости от его цвета
                        отправляю в row_data соответствующую последовательность байтов.  */ 
                uint8_t blue = 0, green = 0, red = 0;

                /*  Избегаю потери качества при сохранении изображения!

                     nothing      blue        green        red
                    0000_0000 ' 0000_0000 ' 0000_0000 ' 0000_0000  */
                blue    = (pixels[y][x] & (255 << 16)) >> 16;
                green   = (pixels[y][x] & (255 << 8)) >> 8;
                red     = (pixels[y][x] & (255));

                // 4.3 Добавляю байты в ряд выходного файла (в правильной последовательности!).
                row_data.push_back(blue);
                row_data.push_back(green);
                row_data.push_back(red);
            }

            // 5. При необходимости, добавляю отступ (padding) для выравнивания строки по 4 байта.
            while (row_data.size() < static_cast<size_t>(row_stride)) {
                row_data.push_back(0);
            }

            // 6. Записываю полученную строку в выходной файл.
            out_file.write(reinterpret_cast<char*>(row_data.data()), row_data.size());
        }

        // 7. Закрываю выходной поток.
        out_file.close();
    }

    // Метод, позволяющий вывести изображение в консоль.
    void printImage() const
    {
        for (int y = 0; y < info_block.height; ++y)
        {
            for (int x = 0; x < info_block.width; ++x)
            {
                // Если пиксель черный:
                if (pixels[y][x] == 0x00'00'00) { std::cout << black; }

                // Если пиксель белый:
                else if (pixels[y][x] == 0xFF'FF'FF) { std::cout << white; }

                // Если пиксель неизвестного мне цвета:
                else { std::cout << unknown_color; }
            }

            std::cout << '\n';
        }
    }
};

int main()
{
    // 1. Создаю объект класса BMPImageEditor для работы с BMP-файлами.
    BMPImageEditor object;

    // 2. Прошу пользователя ввести с клавиатуры путь к входному файлу.
    std::string input_file_path;
    std::cout << "\nEnter input BMP file name: ";
    std::getline(std::cin, input_file_path);

    /* 3.   Данный блок кода я обернул в try, чтобы в случае ошибок 
            пользователь получил корректное и понятное сообщение об ошибке.   */
    try 
    {
        // 3.1 Считываю всю информацию из входного файла.
        object.read(input_file_path);

        // 3.2 Вывожу изображение в консоль.
        std::cout << "\nImage before changes:\n";
        object.printImage();

        // 3.3 Рисую на изображении крест (выбрал оранжевый цвет).
        object.drawCross(0, 165, 255);

        // 3.4 Снова вывожу изображение в консоль.
        std::cout << "\nImage after changes: \n";
        object.printImage();

        // 3.5 Прошу пользователя ввести с клавиатуры путь к выходному файлу.
        std::string output_file_path;
        std::cout << "\nEnter output BMP file name: ";
        std::getline(std::cin, output_file_path);

        // 3.6 Сохраняю измененное изображение в выходной файл.
        object.save(output_file_path);
    }
    catch(const std::exception& object) 
    {
        // В данном блоке кода я обрабатываю возможные исключения.
        std::cerr << object.what();
    }

    return 0;
}