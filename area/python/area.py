#Ошибка, которую вы получаете, указывает на то, что массивы, передаваемые в функцию cv2.getPerspectiveTransform , не имеют правильной формы. Для успешного выполнения этой функции необходимо, чтобы каждый из массивов содержал ровно четыре точки (координаты).

#Давайте добавим проверку на то, что найденные точки (в массиве approx ) содержат ровно 4 точки, которые можно использовать для вычисления матрицы гомографии. Также мы можем добавить обработку ситуации, когда точки не обнаружены или их недостаточно.

#Вот обновленный код с необходимыми проверками:
#pythonСкопировать
#Основные изменения:
#1. Проверка на количество точек: Мы добавили проверку на количество точек в approx . Если количество точек не равно 4, функция возвращает 0 и None для остальных изображений.

#2. Обработка ошибок: Добавлена обработка ошибок при вычислении матрицы гомографии, чтобы избежать падения программы в случае возникновения исключений.

#Эти изменения должны помочь устранить ошибку и корректно обрабатывать ситуации, когда не удается найти квадрат. Если у вас возникнут другие вопросы или ошибки, дайте знать!

#===========
#https://robotclass.ru/tutorials/opencv-python-find-contours/
#https://waksoft.susu.ru/2021/11/30/obnaruzhenie-kongurov-s-ispolzovaniem-opencv/
import cv2
import numpy as np
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk


def get_image_with_contour(image, contour):
    """
    Возвращает изображение, обрезанное по заданному контуру.

    :param image: Входное изображение (numpy.ndarray).
    :param contour: Контур, по которому нужно обрезать изображение (numpy.ndarray).
    :return: Изображение, обрезанное по контуру (numpy.ndarray).
    """
    # Создаем маску такого же размера, как изображение
    mask = np.zeros_like(image)

    # Заполняем маску контуром
    cv2.drawContours(mask, [contour], -1, (255, 255, 255), thickness=cv2.FILLED)

    # Накладываем маску на изображение
    masked_image = cv2.bitwise_and(image, mask)

    return masked_image
#  * * * * * * * *
def calculate_area(image_path):
    image = cv2.imread(image_path)
    if image is None:
        return 0.0, None, None, 0.0

    # Сохраняем оригинальное изображение для отображения
    original_image = image.copy()

    hsv_image = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)

    lower_red1 = np.array([0, 166, 80])
    upper_red1 = np.array([214, 255, 255])
    lower_red2 = np.array([170, 70, 50])
    upper_red2 = np.array([180, 255, 255])

    mask1 = cv2.inRange(hsv_image, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv_image, lower_red2, upper_red2)
    mask_red = cv2.bitwise_or(mask1, mask2)

    # Нахождение контуров на маске
    contours,  hierarchy = cv2.findContours(mask_red, cv2.RETR_TREE, cv2.CHAIN_APPROX_NONE) #, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if not contours:
        return 0.0, None, None, 0.0

    # Находим контур с максимальной площадью
    largest_contour = max(contours, key=cv2.contourArea)
    maxX = 0
    minX = 999999
    maxY = 0
    minY = 999999
    for point in largest_contour:
        x = int(point[0][0])
        y = int(point[0][1])
        if ( x > maxX ) :
            maxX = x
        if ( y > maxY ) :
            maxY = y
        if ( x < minX ) :
            minX = x
        if ( y < minY ) :
            minY = y
    square_image = image[ minY:maxY, minX:maxX]
    pil_sqrimage = Image.fromarray( cv2.cvtColor(square_image, cv2.COLOR_BGR2RGB))
    pil_sqrimage.save('square_image.jpg')

    hsv_sqrimage = cv2.cvtColor(square_image, cv2.COLOR_BGR2HSV)
    mask1 = cv2.inRange(hsv_sqrimage, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv_sqrimage, lower_red2, upper_red2)
    mask_redsqr = cv2.bitwise_or(mask1, mask2)
    mask_non_red = cv2.bitwise_not(mask_redsqr)

#    cv2.drawContours(hsv_image, contours, -1, (255, 0, 0), 3, cv2.LINE_AA, hierarchy, 1)
#    cv2.imshow('contours', hsv_image)

    non_red_contours, hierarchy = cv2.findContours(mask_non_red, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    total_non_red_area = 0.0
    for contour in non_red_contours:
        area_pixels = cv2.contourArea(contour)
        total_non_red_area += area_pixels

    # Изменяем размеры изображений для отображения
    original_image_resized = cv2.resize(original_image, (300, 300))

    total_non_red_area_cm2 = 0.0
    # Создаем изображение с не красными фигурами
    non_red_highlighted = original_image.copy()
    for contour in non_red_contours:
        area_pixels = cv2.contourArea(contour)
        total_non_red_area_cm2 += area_pixels
        cv2.fillPoly(non_red_highlighted, [contour], (0, 255, 0))  # Заполняем не красные фигуры зеленым цветом
    non_red_highlighted_resized = cv2.resize(non_red_highlighted, (300, 300))

    # Преобразуем площадь в квадратные сантиметры
    pixel_per_cm = ( maxY- minY ) / 15 #10.0
    total_non_red_area_cm2 = total_non_red_area_cm2 / (pixel_per_cm ** 2)

    font = cv2.FONT_HERSHEY_COMPLEX
    crMin =  ( total_non_red_area_cm2 * 10. ) / 1000
    crMax =  ( total_non_red_area_cm2 * 20. ) / 1000

    imgNR = cv2.resize(mask_non_red, (300, 300))
    cv2.putText(imgNR, f"площадь: {total_non_red_area_cm2:.0f} см²", ( 20, 20),  font, 0.5, (255, 0, 0))
    cv2.putText(imgNR, f"ток: { crMin:.1f} - { crMax:.1f} A", ( 20, 40),  font, 0.5, (255, 0, 0))
    pil_imageNR = Image.fromarray(imgNR)
    imgnr = ImageTk.PhotoImage(image=pil_imageNR)
    panelnr = tk.Label(root, image=imgnr)
    panelnr.place(x=660, y=50)


    messagebox.showinfo(f"Суммарная площадь фигур: {total_non_red_area_cm2:.2f} см²")
#    return area_cm2, original_image_resized, warped_image_resized, non_red_highlighted_resized, total_non_red_area_cm2
#  * * * * * * * *
def load_image():
    file_path = filedialog.askopenfilename(title="Выберите изображение", filetypes=[("Image files", "*.jpg *.jpeg *.png")])
#    file_path = 'C:/ed/api/prog/areas/l4.jpg'
#    if not file_path:
#        return

    # Загружаем изображение и отображаем его
    image = Image.open(file_path)
    image.thumbnail((300, 300))  # Изменяем размер изображения для отображения
    img = ImageTk.PhotoImage(image)

    label_original.config(image=img)
    label_original.image = img  # Сохраняем ссылку на изображение
    label_original.file_path = file_path  # Сохраняем путь к файлу
#  * * * * * * * *
def calculate_and_display_area():
    if not hasattr(label_original, 'file_path'):
        messagebox.showerror("Ошибка", "Сначала выберите изображение.")
        return

#    area, original_image, warped_image, non_red_highlighted, total_non_red_area =
    calculate_area(label_original.file_path)
#app = tk.Tk()

root = tk.Tk()
root.title("area square")
root.geometry("1200x340")
root.resizable(width=False, height=False)

# Создаем три Label для отображения изображений
label_original = tk.Label(root)
#panel0 = tk.Label(root, image=label_original)
#panel0.place(x=0, y=50)
label_original.pack(side=tk.LEFT, padx=10)
#label_original.place(x=000, y=200)

label_warped = tk.Label(root)
label_warped.pack(side=tk.LEFT, padx=10)

label_non_red = tk.Label(root)
label_non_red.pack(side=tk.LEFT, padx=10)

btn_load = tk.Button(root, text="Загрузить изображение", command=load_image)
btn_load.pack(pady=10)
#load_image()


btn_calculate = tk.Button(root, text="Вычислить площадь", command=calculate_and_display_area)
btn_calculate.pack(pady=10)
# Запускаем главный цикл приложения
root.mainloop()
