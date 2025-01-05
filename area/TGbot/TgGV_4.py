#pip install pyTelegramBotAPI
import cv2
import datetime
import os
import telebot
from telebot import types

from PIL import Image
import numpy as np

from tgtoken import GV_token
TOKEN = GV_token
bot = telebot.TeleBot(TOKEN)

@bot.message_handler(commands=['start'])
def start(message):
    # Создаем клавиатуру
    keyboard = types.ReplyKeyboardMarkup(resize_keyboard=True)
    area_button = types.KeyboardButton("Area")
    keyboard.add(area_button)
    bot.reply_to(message, 'Привет! Нажмите кнопку "Area" или отправьте изображение, и я определю площадь.', reply_markup=keyboard)

@bot.message_handler(commands=['apk'])
def send_apk(message):
    with open('galvanica.apk', 'rb') as f:
        bot.send_document(message.chat.id, f)

@bot.message_handler(commands=['time'])
def current_time(message):
    now = datetime.datetime.now().strftime("%H:%M:%S")
    bot.reply_to(message, f'Текущее время: {now}')

@bot.message_handler(content_types=['text'])
def handle_text(message):
    if message.text == "Area":
        bot.reply_to(message, 'Вы нажали кнопку "Area". Пожалуйста, отправьте изображение для обработки.')

@bot.message_handler(content_types=['photo'])
def handle_photo(message):
    # Получаем файл изображения
    file_info = bot.get_file(message.photo[-1].file_id)
    downloaded_file = bot.download_file(file_info.file_path)

    with open('received_image.jpg', 'wb') as new_file:
        new_file.write(downloaded_file)

    output_file_path = countArea('received_image.jpg')

    with open(output_file_path, 'rb') as f:
        bot.send_photo(message.chat.id, f)

    # Удаляем временные файлы
    os.remove('received_image.jpg')
    os.remove(output_file_path)

def add_date_to_image(file_path: str) -> str:
    # Загружаем изображение
    image = cv2.imread(file_path)

    # Получаем текущую дату
    current_date = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Добавляем текст на изображение
    font = cv2.FONT_HERSHEY_SIMPLEX
    cv2.putText(image, current_date, (10, 30), font, 1, (255, 255, 255), 2, cv2.LINE_AA)

    # Сохраняем измененное изображение
    output_file_path = 'output_with_date.jpg'
    cv2.imwrite(output_file_path, image)

    return output_file_path
# * * * * * * * * *
def countArea(image_path: str) -> str:
    image = cv2.imread(image_path)
    hsv_image = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)

    lower_red1 = np.array([0, 166, 80])
    upper_red1 = np.array([214, 255, 255])
    lower_red2 = np.array([170, 70, 50])
    upper_red2 = np.array([180, 255, 255])

    mask1 = cv2.inRange(hsv_image, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv_image, lower_red2, upper_red2)
    mask_red = cv2.bitwise_or(mask1, mask2)
    contours,  hierarchy = cv2.findContours(mask_red, cv2.RETR_TREE, cv2.CHAIN_APPROX_NONE) #, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if not contours:
        return image_path
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

    total_non_red_area_cm2 = 0.0
    for contour in non_red_contours:
        area_pixels = cv2.contourArea(contour)
        total_non_red_area_cm2 += area_pixels

    pixel_per_cm = ( maxY- minY ) / 15 #10.0
    total_non_red_area_cm2 = total_non_red_area_cm2 / (pixel_per_cm ** 2)

    font = cv2.FONT_HERSHEY_TRIPLEX
    crMin =  ( total_non_red_area_cm2 *  5. ) / 1000 # 10.
    crMax =  ( total_non_red_area_cm2 * 15. ) / 1000 # 20.

    imgNR_ = cv2.resize(mask_non_red, (600, 600))
    imgNR = cv2.cvtColor(imgNR_, cv2.COLOR_GRAY2RGB)

    color = (255, 0, 0) # RGB
    thickness = 2
    cv2.putText(imgNR, f"area : {total_non_red_area_cm2:.0f} cm^2", ( 30, 50),  font, 1., color, thickness)
    cv2.putText(imgNR, f"current: { crMin:.1f} - { crMax:.1f} A", ( 40, 80),  font, 1., color, thickness)
    pil_imageNR = Image.fromarray(imgNR)
    pil_imageNR.save('area_image.jpg')
    # Удаляем временные файлы
    os.remove('received_image.jpg')
    return 'area_image.jpg'

# = = =  = = = = = = = = = = = = = = = = = = = = = =
if __name__ == '__main__':
    bot.polling(none_stop=True)
