��          �   %   �      `     a     v     �     �     �  �   �  �   q  �   (  �   �  �   �  -   M  @   {  F   �  J       N  	   j     t  3  �  	   �
     �
  
   �
  �   �
     x  	   �     �  d  �  8    <   D     �     �  9   �     �  �   �  �   �  �   Z  �     �   �  @   s  ^   �  W     L   k  >  �  )   �     !  �  =  .        E  2   I  �   |       *   %  8   P  �  �                                    
           	                                                                             <b>Computer data</b> <b>Mountpoint</b> <b>Partition</b> <b>Personal data</b> <b>Size</b> <span foreground="#9F6C49"> <b>Intro</b> > <b>Identification</b> > <b>Disk space</b> > <b>Installation</b> > <span foreground="white" background="#9F6C49"><b>Ready!</b></span></span> <span foreground="#9F6C49"> <b>Intro</b> > <b>Identification</b> > <b>Disk space</b> > <span foreground="white" background="#9F6C49"><b>Installation</b></span> > <b>Ready!</b></span> <span foreground="#9F6C49"> <b>Intro</b> > <b>Identification</b> > <span foreground="white" background="#9F6C49"><b>Disk space</b></span> > <b>Installation</b> > <b>Ready!</b></span> <span foreground="#9F6C49"> <b>Intro</b> > <span foreground="white" background="#9F6C49"><b>Identification</b></span> > <b>Disk space</b> > <b>Installation</b> > <b>Ready!</b></span> <span foreground="#9F6C49"> <span foreground="white" background="#9F6C49"><b>Intro</b></span> > <b>Identification</b> > <b>Disk space</b> > <b>Installation</b> > <b>Ready!</b></span> <span size="xx-large">Congratulations!</span> <span size="xx-large">Identify yourself and your computer</span> <span size="xx-large">Introducing your forthcoming free desktop</span> <span size="xx-large">Please prepare some space for your new system</span> <span>Es necesario que introduzca su <b>nombre de usuario</b> para el sistema, su <b>nombre completo</b> para generar una ficha de usuario, así como el <b>nombre de máquina</b> con el que quiera bautizar su equipo. Deberá teclear la contraseña de usuario en dos ocasiones.</span> Hostname: Installation progress Now you should have at least 3 GNU/Linux partitions with space enough. It's time to specify where you want every component to be installed.
Proceed as follows: select one partition in the column on the left and link it with one mount point on the right. Repeat for every desired partition.
There are 3 necessary “mount points” – they must be associated with 3 partitions. They are “/”, “/home” and “swap”.
If you kept an intact home partition, you can now link it with the “/home” mount point. It will not be formatted, so data is preserved. Password: Ready Real Name: The installation has been successfully completed.

In order to work with your new free operating system you need to eject the CDLive and reboot your computer. Ubiquity Installer Username: Verify password: You have to make room in one or more of your hard disks in order to have GNU/Linux properly installed. 3 partitions are necessary:
· The root partition (“/”), with a minimum size of 1.5 GB.
· The home partition (“/home”), of 512 MB at least.
· The swap partition (“swap”), with 256 MB or more.
You can now modify your existing partition table and select where you wan what.
Remember that you can keep the data in a previous home partition – just leave it as it is and it will not be formatted.
Alternatively, it is possible to return to the previous page to select an easier partitioning method. Project-Id-Version: Ubuntu Express 1.0
Report-Msgid-Bugs-To: 
POT-Creation-Date: 2005-11-30 08:05+0100
PO-Revision-Date: 2005-11-30 08:05+0100
Last-Translator: FULL NAME <EMAIL@ADDRESS>
Language-Team: LANGUAGE <LL@li.org>
MIME-Version: 1.0
Content-Type: text/plain; charset=UTF-8
Content-Transfer-Encoding: 8bit
 <span foreground='#90b5f9'><b>Datos del ordenador</b></span> <b>Punto de montaje</b> <b>Partición</b> <span foreground='#90b5f9'><b>Datos personales</b></span> <b>Tamaño</b> <span foreground="#3b5da5"> <b>Intro</b> > <b>Identificación</b> > <b>Disco</b> > <b>Instalación</b> > <span foreground="white" background="#3b5da5"><b>¡Fin!</b></span></span> <span foreground="#3b5da5"> <b>Intro</b> > <b>Identificación</b> > <b>Disco</b> > <span foreground="white" background="#3b5da5"><b>Instalación</b></span> > <b>¡Fin!</b></span> <span foreground="#3b5da5"> <b>Intro</b> > <b>Identificación</b> > <span foreground="white" background="#3b5da5"><b>Disco</b></span> > <b>Instalación</b> > <b>¡Fin!</b></span> <span foreground="#3b5da5"> <b>Intro</b> > <span foreground="white" background="#3b5da5"><b>Identificación</b></span> > <b>Disco</b> > <b>Instalación</b> > <b>¡Fin!</b></span> <span foreground="#3b5da5"> <span foreground="white" background="#3b5da5"><b>Intro</b></span> > <b>Identificación</b> > <b>Disco</b> > <b>Instalación</b> > <b>¡Fin!</b></span> <span size="xx-large" foreground="#3b5da5">¡Felicidades!</span> <span size="xx-large" foreground="#3b5da5">Identifíquese y ponga nombre a su ordenador</span> <span size="xx-large" foreground="#3b5da5">Cómo será su nuevo escritorio libre</span> <span size="xx-large" foreground="#3b5da5">¿Donde ubicar Guadalinex?</span> El nuevo sistema necesita su nombre completo (ej: María Sánchez), su nombre de usuario (ej: msanchez) y una contraseña que sólo usted conozca y sea sencilla de recordar.
También precisa un nombre para su ordenador, que debe constar de una única palabra sin acentos, eñes o caracteres especiales (ej: Grazalema). <span foreground='#3b5da5'>Nombre</span>  Progreso de la instalación Ahora sólo falta que le asigne a las nuevas particiones un punto de montaje. Es decir, un directorio dentro del sistema de ficheros:
· / para la partición de directorio raíz
· /swap para la partición de intercambio
· /home para la partición de archivos de los usuarios
Puede definir el punto de montaje que desee para las particiones adicionales que haya creado para su propio uso (ej: /copias).
Una vez definidos todos los puntos de montaje puede seguir adelante. <span foreground='#3b5da5'>Contraseña</span>  Fin <span foreground='#3b5da5'>Nombre completo</span>  El proceso de instalación se ha completado.

Para utilizar su nuevo sistema operativo libre es necesario extraer el CDLive y reiniciar su ordenador. Guadalinex Express <span foreground='#3b5da5'>Usuario</span>  <span foreground='#3b5da5'>Verifique contraseña</span>  Con este particionador manual puede crear, borrar y redimensionar particiones. No se lo tome a la ligera: su uso indebido puede conllevar la eliminación de datos valiosos existentes en el disco duro.
Se recomiendan tres particiones para el nuevo sistema, que puede obtener de su espacio libre en disco o de particiones ya existentes:
· La partición raíz ("/"), donde el nuevo sistema será instalado. Requiere un mínimo de 1,5Gb.
· La partición /swap, utilizada por el sistema para su propio funcionamiento. Tiene suficiente con 256Mb.
· La partición /home es donde se ubican los documentos y ficheros de los usuarios. El mínimo recomendado es de 500Mb y cuanto más espacio le pueda asignar, mejor. Si ya tiene una partición /home de otro sistema Linux puede conservarla para el nuevo sistema, sin necesidad de cambio alguno.
Si tiene más espacio disponible puede crear más particiones. Cada partición es interpretada por el sistema como un disco independiente, por lo que puede tener por ejemplo una partición /copias para guardar copias de seguridad de sus archivos.
Siga adelante para efectuar los cambios en la tabla de particiones o retroceda para realizar un particionamiento asistido. 