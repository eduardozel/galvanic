#https://robotclass.ru/tutorials/opencv-python-find-contours/
#https://waksoft.susu.ru/2021/11/30/obnaruzhenie-kongurov-s-ispolzovaniem-opencv/
import cv2
import numpy as np
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk



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

    non_red_contours, hierarchy = cv2.findContours(mask_non_red, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    total_non_red_area = 0.0
    # Создаем изображение с не красными фигурами
    for contour in non_red_contours:
        area_pixels = cv2.contourArea(contour)
        total_non_red_area += area_pixels

    # Преобразуем площадь в квадратные сантиметры
    pixel_per_cm = ( maxY- minY ) / 15 #10.0
    total_non_red_area = total_non_red_area / (pixel_per_cm ** 2)

    font = cv2.FONT_HERSHEY_COMPLEX
    crMin =  ( total_non_red_area * 10. ) / 1000
    crMax =  ( total_non_red_area * 20. ) / 1000

    imgNR_ = cv2.resize(mask_non_red, (300, 300))
    imgNR = cv2.cvtColor(imgNR_, cv2.COLOR_GRAY2RGB)
    cv2.putText(imgNR, f"площадь: {total_non_red_area:.0f} см²", ( 20, 20),  font, 0.5, (255, 0, 0))
    cv2.putText(imgNR, f"ток: { crMin:.1f} - { crMax:.1f} A", ( 20, 40),  font, 0.5, (255, 0, 0))
    pil_imageNR = Image.fromarray(imgNR)
    imgnr = ImageTk.PhotoImage(image=pil_imageNR)

    label_figures.config(image=imgnr)
    label_figures.image = imgnr
    label_area = tk.Label( text=f"площадь фигур: {total_non_red_area:.0f} см²", borderwidth=2, bg="White", highlightthickness=4, highlightbackground="#37d3ff")
    label_area.place(x=340, y=0)

#  * * * * * * * *
def calculate_and_display_area():
    if not hasattr(label_original, 'file_path'):
        messagebox.showerror("Ошибка", "Сначала выберите изображение.")
        return
    calculate_area(label_original.file_path)

def load_image():
    file_path = filedialog.askopenfilename(title="Выберите изображение", filetypes=[("Image files", "*.jpg *.jpeg *.png")])
#    file_path = 'C:/ed/api/prog/areas/l4.jpg'
#    if not file_path:
#        return

    image = Image.open(file_path)
    image.thumbnail((300, 300))
    img = ImageTk.PhotoImage(image)

    label_original.config(image=img)
    label_original.image = img
    label_original.file_path = file_path

    calculate_and_display_area()
#  * * * * * * * * * * * * * *
# ** ** ** ** MAIN ** ** ** **
root = tk.Tk()
root.title("area figures")
root.geometry("640x360")
root.resizable(width=False, height=False)

label_original = tk.Label(root)
label_original.place(x=000, y=40)

label_figures = tk.Label(root)
label_figures.place(x=320, y=40)

btn_load = tk.Button(root, text="Загрузить изображение", command=load_image)
btn_load.place(x=20, y=10)
# = = = = = = = = = =
root.mainloop()
